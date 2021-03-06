#include "Z80Instruction.h"
#ifndef __Z80_IM_H__
#define __Z80_IM_H__
namespace Z80 {

class IM :public Z80Instruction
{
public:
	virtual IM* Clone() const {return new IM();}
	virtual IM* ClonePlacement(void* buffer) const {return new(buffer) IM();}
	virtual size_t GetOpcodeClassByteSize() const {return sizeof(*this);}

	virtual bool RegisterOpcode(OpcodeTable<Z80Instruction>& table) const
	{
		bool result = true;
		result &= table.AllocateRegionToOpcode(this, L"01000110", L"");
		result &= table.AllocateRegionToOpcode(this, L"01010110", L"");
		result &= table.AllocateRegionToOpcode(this, L"01011110", L"");
		return result;
	}

	virtual std::wstring GetOpcodeName() const
	{
		return L"IM";
	}

	virtual Disassembly Z80Disassemble(const Z80::LabelSubstitutionSettings& labelSettings) const
	{
		std::wstringstream argumentDisassembly;
		argumentDisassembly << _newInterruptMode;
		return Disassembly(GetOpcodeName(), argumentDisassembly.str());
	}

	virtual void Z80Decode(Z80* cpu, const Z80Word& location, const Z80Byte& data, bool transparent)
	{
		//##TODO## Determine which mode the undocumented "IM 0/1*" modes actually set
		switch (data.GetDataSegment(3, 2))
		{
		case 0:
			_newInterruptMode = 0;
			break;
		case 2:
			_newInterruptMode = 1;
			break;
		case 3:
			_newInterruptMode = 2;
			break;
		}
		AddExecuteCycleCount(4);
	}

	virtual ExecuteTime Z80Execute(Z80* cpu, const Z80Word& location) const
	{
		// Perform the operation
		cpu->SetInterruptMode(_newInterruptMode);

		// Adjust the PC and return the execution time
		cpu->SetPC(location + GetInstructionSize());
		return GetExecuteCycleCount();
	}

private:
	unsigned int _newInterruptMode;
};

} // Close namespace Z80
#endif
