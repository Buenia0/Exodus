#include "Z80Instruction.h"
#ifndef __Z80_NOP_H__
#define __Z80_NOP_H__
namespace Z80 {

class NOP :public Z80Instruction
{
public:
	virtual NOP* Clone() const {return new NOP();}
	virtual NOP* ClonePlacement(void* buffer) const {return new(buffer) NOP();}
	virtual size_t GetOpcodeClassByteSize() const {return sizeof(*this);}

	virtual bool RegisterOpcode(OpcodeTable<Z80Instruction>& table) const
	{
		return table.AllocateRegionToOpcode(this, L"00000000", L"");
	}

	virtual std::wstring GetOpcodeName() const
	{
		return L"NOP";
	}

	virtual Disassembly Z80Disassemble(const Z80::LabelSubstitutionSettings& labelSettings) const
	{
		return Disassembly(GetOpcodeName(), L"");
	}

	virtual void Z80Decode(Z80* cpu, const Z80Word& location, const Z80Byte& data, bool transparent)
	{
		AddExecuteCycleCount(4);
	}

	virtual ExecuteTime Z80Execute(Z80* cpu, const Z80Word& location) const
	{
		// Adjust the PC and return the execution time
		cpu->SetPC(location + GetInstructionSize());
		return GetExecuteCycleCount();
	}
};

} // Close namespace Z80
#endif
