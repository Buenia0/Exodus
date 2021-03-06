#ifndef __IGENERICACCESSGROUP_H__
#define __IGENERICACCESSGROUP_H__
#include "IGenericAccessGroupEntry.h"
#include "MarshalSupport/MarshalSupport.pkg"
#include <string>
#include <list>
using namespace MarshalSupport::Operators;

class IGenericAccessGroup :public IGenericAccessGroupEntry
{
public:
	// Interface version functions
	static inline unsigned int ThisIGenericAccessGroupVersion() { return 1; }
	virtual unsigned int GetIGenericAccessGroupVersion() const = 0;

	// Group info functions
	virtual Marshal::Ret<std::wstring> GetName() const = 0;
	virtual bool GetOpenByDefault() const = 0;

	// Entry functions
	virtual unsigned int GetEntryCount() const = 0;
	virtual Marshal::Ret<std::list<IGenericAccessGroupEntry*>> GetEntries() const = 0;

protected:
	// Parent functions
	inline void SetParentForTargetEntry(IGenericAccessGroupEntry* entry, IGenericAccessGroup* parent) const;
};

#include "IGenericAccessGroup.inl"
#endif
