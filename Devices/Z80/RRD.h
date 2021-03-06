#include "Z80Instruction.h"
#ifndef __Z80_RRD_H__
#define __Z80_RRD_H__
namespace Z80 {

class RRD :public Z80Instruction
{
public:
	virtual RRD* Clone() const {return new RRD();}
	virtual RRD* ClonePlacement(void* buffer) const {return new(buffer) RRD();}
	virtual size_t GetOpcodeClassByteSize() const {return sizeof(*this);}

	virtual bool RegisterOpcode(OpcodeTable<Z80Instruction>& table) const
	{
		return table.AllocateRegionToOpcode(this, L"01100111", L"");
	}

	virtual std::wstring GetOpcodeName() const
	{
		return L"RRD";
	}

	virtual Disassembly Z80Disassemble(const Z80::LabelSubstitutionSettings& labelSettings) const
	{
		return Disassembly(GetOpcodeName(), L"");
	}

	virtual void Z80Decode(Z80* cpu, const Z80Word& location, const Z80Byte& data, bool transparent)
	{
		_hl.SetIndexState(GetIndexState(), GetIndexOffset());
		_a.SetIndexState(GetIndexState(), GetIndexOffset());

		// RRD		11101101 01100111
		_hl.SetMode(EffectiveAddress::Mode::HLIndirect);
		_a.SetMode(EffectiveAddress::Mode::A);
		AddExecuteCycleCount(14);

		AddInstructionSize(GetIndexOffsetSize(_hl.UsesIndexOffset() || _a.UsesIndexOffset()));
		AddInstructionSize(_hl.ExtensionSize());
		AddInstructionSize(_a.ExtensionSize());
	}

	virtual ExecuteTime Z80Execute(Z80* cpu, const Z80Word& location) const
	{
		double additionalTime = 0;
		Z80Byte hlSource;
		Z80Byte aSource;
		Z80Byte hlResult;
		Z80Byte aResult;

		// Perform the operation
		additionalTime += _hl.Read(cpu, location, hlSource);
		additionalTime += _a.Read(cpu, location, aSource);

		aResult = aSource;
		aResult.SetDataSegment(0, 4, hlSource.GetDataSegment(0, 4));
		hlResult.SetDataSegment(4, 4, aSource.GetDataSegment(0, 4));
		hlResult.SetDataSegment(0, 4, hlSource.GetDataSegment(4, 4));

		additionalTime += _hl.Write(cpu, location, hlResult);
		additionalTime += _a.Write(cpu, location, aResult);

		// Set the flag results
		cpu->SetFlagS(aResult.Negative());
		cpu->SetFlagZ(aResult.Zero());
		cpu->SetFlagY(aResult.GetBit(5));
		cpu->SetFlagH(false);
		cpu->SetFlagX(aResult.GetBit(3));
		cpu->SetFlagPV(aResult.ParityEven());
		cpu->SetFlagN(false);

		// Adjust the PC and return the execution time
		cpu->SetPC(location + GetInstructionSize());
		return GetExecuteCycleCount(additionalTime);
	}

private:
	EffectiveAddress _hl;
	EffectiveAddress _a;
};

} // Close namespace Z80
#endif
