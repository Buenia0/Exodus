#include "Z80Instruction.h"
#ifndef __Z80_OR8_H__
#define __Z80_OR8_H__
namespace Z80 {

class OR8 :public Z80Instruction
{
public:
	virtual OR8* Clone() const {return new OR8();}
	virtual OR8* ClonePlacement(void* buffer) const {return new(buffer) OR8();}
	virtual size_t GetOpcodeClassByteSize() const {return sizeof(*this);}

	virtual bool RegisterOpcode(OpcodeTable<Z80Instruction>& table) const
	{
		bool result = true;
		result &= table.AllocateRegionToOpcode(this, L"10110***", L"");
		result &= table.AllocateRegionToOpcode(this, L"11110110", L"");
		return result;
	}

	virtual std::wstring GetOpcodeName() const
	{
		return L"OR";
	}

	virtual Disassembly Z80Disassemble(const Z80::LabelSubstitutionSettings& labelSettings) const
	{
		return Disassembly(GetOpcodeName(), _target.Disassemble() + L", " + _source.Disassemble());
	}

	virtual void Z80Decode(Z80* cpu, const Z80Word& location, const Z80Byte& data, bool transparent)
	{
		_source.SetIndexState(GetIndexState(), GetIndexOffset());
		_target.SetIndexState(GetIndexState(), GetIndexOffset());
		_target.SetMode(EffectiveAddress::Mode::A);

		if (_source.Decode8BitRegister(data.GetDataSegment(0, 3)))
		{
			// OR A,r		10110rrr
			AddExecuteCycleCount(4);
		}
		else if (data.GetBit(6))
		{
			// OR A,n		11110110
			_source.BuildImmediateData(BITCOUNT_BYTE, location + GetInstructionSize(), cpu, transparent);
			AddExecuteCycleCount(7);
		}
		else
		{
			// OR A,(HL)		10110110
			// OR A,(IX + d)	11011101 10110110 dddddddd
			// OR A,(IY + d)	11111101 10110110 dddddddd
			_source.SetMode(EffectiveAddress::Mode::HLIndirect);
			if (GetIndexState() == EffectiveAddress::IndexState::None)
			{
				AddExecuteCycleCount(7);
			}
			else
			{
				AddExecuteCycleCount(15);
			}
		}

		AddInstructionSize(GetIndexOffsetSize(_source.UsesIndexOffset() || _target.UsesIndexOffset()));
		AddInstructionSize(_source.ExtensionSize());
		AddInstructionSize(_target.ExtensionSize());
	}

	virtual ExecuteTime Z80Execute(Z80* cpu, const Z80Word& location) const
	{
		double additionalTime = 0;
		Z80Byte op1;
		Z80Byte op2;
		Z80Byte result;

		// Perform the operation
		additionalTime += _source.Read(cpu, location, op1);
		additionalTime += _target.Read(cpu, location, op2);
		result = op2 | op1;
		additionalTime += _target.Write(cpu, location, result);

		// Set the flag results
		cpu->SetFlagS(result.Negative());
		cpu->SetFlagZ(result.Zero());
		cpu->SetFlagY(result.GetBit(5));
		cpu->SetFlagH(false);
		cpu->SetFlagX(result.GetBit(3));
		cpu->SetFlagPV(result.ParityEven());
		cpu->SetFlagN(false);
		cpu->SetFlagC(false);

		// Adjust the PC and return the execution time
		cpu->SetPC(location + GetInstructionSize());
		return GetExecuteCycleCount(additionalTime);
	}

private:
	EffectiveAddress _source;
	EffectiveAddress _target;
};

} // Close namespace Z80
#endif
