#ifndef __M68000_EFFECTIVEADDRESS_H__
#define __M68000_EFFECTIVEADDRESS_H__
#include "M68000.h"
namespace M68000 {

// Note that the current value of the Program Counter is important when decoding the
// effective address target. Any targets which use PC-relative addressing need to know what
// the updated PC is during the decoding process. For simple opcodes where the PC-relative
// address is defined by the extension word immediately following the 2-byte opcode, or for
// opcodes where the effective address is implied by the 2-byte opcode itself, the correct
// PC address to use for decoding the relative address is the location of the opcode, plus
// 2 bytes. In the case of opcodes where the extension word which uses a PC-relative
// address doesn't immediately follow the 2-byte opcode however, the value of the program
// counter may be more than the opcode location + 2 at the time the effective address is
// decoded.

// After further reading, the rule is that the value of the PC used in effective address
// calculations is the address of the extension word that defines the effective address.
// See the M68000 Programmer's Manual, Section 2.2.11. "The value in the PC is the address
// of the extension word." In cases where a particular effective address is implied by an
// opcode, the address is the next 2 bytes following the simple effective address
// operation word (instruction register). It follows that the same PC value should be
// pushed to the stack in the case of a group 0 exception when accessing memory through an
// effective address.

// As per the notes used to say in the MOVEM opcode, MOVEM, CMPI, and BTST are some
// opcodes which rely on the PC relative address decoding being done correctly. Any opcode
// which has a PC-relative address will at least require the PC to be incremented by 2
// when calculating the new address. MOVEM, CMPI, and BTST are some instructions where the
// PC value may be incremented by more than 2 when the PC relative address is calculated.
// IIRC, these may be the only opcodes for the base M68000 where this case can occur.

class EffectiveAddress
{
public:
	// Enumerations
	enum class Mode;

	// Constructors
	inline EffectiveAddress();

	// Decode functions
	void Decode(unsigned int reg, unsigned int mode, Bitcount size, const M68000Long& location, M68000* cpu, bool transparent, const M68000Word& instructionRegister);
	inline void BuildDataDirect(Bitcount size, const M68000Long& location, unsigned int reg);
	inline void BuildAddressDirect(Bitcount size, const M68000Long& location, unsigned int reg);
	inline void BuildAddressIndirect(Bitcount size, const M68000Long& location, unsigned int reg);
	inline void BuildAddressPostinc(Bitcount size, const M68000Long& location, unsigned int reg);
	inline void BuildAddressPredec(Bitcount size, const M68000Long& location, unsigned int reg);
	inline void BuildAddressIndirectDisplace(Bitcount size, const M68000Long& location, unsigned int reg, M68000* cpu, bool transparent, const M68000Word& instructionRegister);
	inline void BuildAddressIndirectIndex(Bitcount size, const M68000Long& location, unsigned int reg, M68000* cpu, bool transparent, const M68000Word& instructionRegister);
	inline void BuildAbsoluteAddressWord(Bitcount size, const M68000Long& location, M68000* cpu, bool transparent, const M68000Word& instructionRegister);
	inline void BuildAbsoluteAddressLong(Bitcount size, const M68000Long& location, M68000* cpu, bool transparent, const M68000Word& instructionRegister);
	inline void BuildPCIndirectDisplace(Bitcount size, const M68000Long& location, M68000* cpu, bool transparent, const M68000Word& instructionRegister);
	inline void BuildPCIndirectIndex(Bitcount size, const M68000Long& location, M68000* cpu, bool transparent, const M68000Word& instructionRegister);
	inline void BuildImmediateData(Bitcount size, const M68000Long& location, M68000* cpu, bool transparent, const M68000Word& instructionRegister, bool signExtended = false);
	inline void BuildImmediateData(const M68000Long& location, const Data& data, bool signExtended = false);
	inline void BuildQuickData(const M68000Long& location, unsigned int data);

	// Address functions
	inline M68000Long ExtractProcessedImmediateData() const;
	inline M68000Long GetSavedPC() const;
	inline bool IncrementAddress(Bitcount size);
	bool GetAddress(const M68000* cpu, Data& address) const;
	bool GetAddressTransparent(Data& address) const;
	bool GetAddressDisplacementTargetNoIndex(M68000* cpu, Data& address) const;

	// Extension word info
	inline unsigned int ExtensionSize();
	inline ExecuteTime DecodeTime();
	inline Mode GetAddressMode() const;

	// Data transfer
	double Read(M68000* cpu, Data& target, const M68000Word& instructionRegister, bool rmwCycleInProgress = false, bool rmwCycleFirstOperation = false) const;
	double Write(M68000* cpu, const Data& target, const M68000Word& instructionRegister, bool rmwCycleInProgress = false, bool rmwCycleFirstOperation = false, bool dataUnmodifiedFromMemoryRead = false, unsigned int sourceMemoryAddress = 0, unsigned int sourceMemorySize = 0) const;
	double ReadWithoutAdjustingAddress(M68000* cpu, Data& target, const M68000Word& instructionRegister, bool rmwCycleInProgress = false, bool rmwCycleFirstOperation = false) const;
	bool IsTargetUnmodifiedFromMemoryRead(M68000* cpu, Bitcount readSize, unsigned int& sourceMemoryAddress) const;
	unsigned int GetTargetOriginalMemoryReadSize(M68000* cpu, Bitcount readBitcount) const;
	bool IsTargetUnmodifiedFromMemoryReadV2(M68000* cpu, unsigned int& sourceMemoryAddress, bool& dataIsOffset, unsigned int& offsetBaseAddress, unsigned int& dataSize) const;

	// Disassembly functions
	std::wstring Disassemble(const M68000::LabelSubstitutionSettings& labelSettings) const;
	std::wstring DisassembleImmediateAsPCDisplacement(const M68000::LabelSubstitutionSettings& labelSettings) const;
	std::wstring DisassembleImmediateAsPCDisplacementTargetAddressString() const;
	void AddLabelTargetsToSet(std::set<unsigned int>& labelTargetLocations) const;

private:
	// Extension word size tables
	static const unsigned int ExtensionSize8[];
	static const unsigned int ExtensionSize32[];

	// Decode time tables
	static const ExecuteTime ExecuteTime8[];
	static const ExecuteTime ExecuteTime32[];

private:
	Bitcount _size;
	Mode _mode;
	unsigned int _reg;
	M68000Word _address;
	Data _data;
	bool _dataSignExtended;
	bool _useAddressRegister;
	Bitcount _indexSize;
	unsigned int _indexReg;
	Data _displacement;
	M68000Long _savedPC;
};

} // Close namespace M68000
#include "EffectiveAddress.inl"
#endif
