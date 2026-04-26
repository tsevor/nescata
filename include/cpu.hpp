#pragma once

#include <cstdint>
#include <iostream>

#include "bus.hpp"

union StatusRegister {
	struct {
		uint8_t C: 1; // Carry
		uint8_t Z: 1; // Zero
		uint8_t I: 1; // Interrupt Disable
		uint8_t D: 1; // Decimal
		uint8_t B: 1; // Break
		uint8_t U: 1; // Unused
		uint8_t V: 1; // Overflow
		uint8_t N: 1; // Negative
	};
	uint8_t raw;
};



class CPU {
private:

	// ENUMS
	
	enum AccessType {
		READ,
		WRITE,
		RMW
	};
	
	enum AddressingMode {
		IMP, // implied
		ACC, // accumulator
		IMM, // immediate
		ZPG, // zeropage
		ZPX, // zeropage,X
		ZPY, // zeropage,Y
		REL, // relative
		ABS, // absolute
		ABX, // absolute,X
		ABY, // absolute,Y
		IND, // indirect
		INX, // (indirect,X)
		INY, // (indirect),Y
	};

	enum InterruptVector {
		VECTOR_NMI,
		VECTOR_BRK,
		VECTOR_RESET,
		VECTOR_IRQ,
	};

	// OPCODE LOOKUP TABLES

	// This table holds the BASE cycle counts for each instruction.
	// Additional cycles for page crosses or taken branches are added conditionally.
	inline static const uint8_t OPCODE_CYCLES_MAP[256] = {
	//  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
		7, 6, 0, 8, 3, 3, 5, 5, 3, 2, 2, 2, 4, 4, 6, 6, // 0
		2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7, // 1
		6, 6, 0, 8, 3, 3, 5, 5, 4, 2, 2, 2, 4, 4, 6, 6, // 2
		2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7, // 3
		6, 6, 0, 8, 3, 3, 5, 5, 3, 2, 2, 2, 3, 4, 6, 6, // 4
		2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7, // 5
		6, 6, 0, 8, 3, 3, 5, 5, 4, 2, 2, 2, 5, 4, 6, 6, // 6
		2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7, // 7
		2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4, // 8
		2, 6, 0, 6, 4, 4, 4, 4, 2, 5, 2, 5, 5, 5, 5, 5, // 9
		2, 6, 2, 6, 3, 3, 3, 3, 2, 2, 2, 2, 4, 4, 4, 4, // A
		2, 5, 0, 5, 4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 4, 4, // B
		2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6, // C
		2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7, // D
		2, 6, 2, 8, 3, 3, 5, 5, 2, 2, 2, 2, 4, 4, 6, 6, // E
		2, 5, 0, 8, 4, 4, 6, 6, 2, 4, 2, 7, 4, 4, 7, 7, // F
	};

	inline static const char* OPCODE_MNEMONIC_MAP[256] = {
	//  0      1      2      3      4      5      6      7      8      9      A      B      C      D      E      F
		"BRK", "ORA", "JAM", "SLO", "NOP", "ORA", "ASL", "SLO", "PHP", "ORA", "ASL", "ANC", "NOP", "ORA", "ASL", "SLO",
		"BPL", "ORA", "JAM", "SLO", "NOP", "ORA", "ASL", "SLO", "CLC", "ORA", "NOP", "SLO", "NOP", "ORA", "ASL", "SLO",
		"JSR", "AND", "JAM", "RLA", "BIT", "AND", "ROL", "RLA", "PLP", "AND", "ROL", "ANC", "BIT", "AND", "ROL", "RLA",
		"BMI", "AND", "JAM", "RLA", "NOP", "AND", "ROL", "RLA", "SEC", "AND", "NOP", "RLA", "NOP", "AND", "ROL", "RLA",
		"RTI", "EOR", "JAM", "SRE", "NOP", "EOR", "LSR", "SRE", "PHA", "EOR", "LSR", "ALR", "JMP", "EOR", "LSR", "SRE",
		"BVC", "EOR", "JAM", "SRE", "NOP", "EOR", "LSR", "SRE", "CLI", "EOR", "NOP", "SRE", "NOP", "EOR", "LSR", "SRE",
		"RTS", "ADC", "JAM", "RRA", "NOP", "ADC", "ROR", "RRA", "PLA", "ADC", "ROR", "ARR", "JMP", "ADC", "ROR", "RRA",
		"BVS", "ADC", "JAM", "RRA", "NOP", "ADC", "ROR", "RRA", "SEI", "ADC", "NOP", "RRA", "NOP", "ADC", "ROR", "RRA",
		"NOP", "STA", "NOP", "SAX", "STY", "STA", "STX", "SAX", "DEY", "NOP", "TXA", "ANE", "STY", "STA", "STX", "SAX",
		"BCC", "STA", "JAM", "SHA", "STY", "STA", "STX", "SAX", "TYA", "STA", "TXS", "TAS", "SHY", "STA", "SHX", "SHA",
		"LDY", "LDA", "LDX", "LAX", "LDY", "LDA", "LDX", "LAX", "TAY", "LDA", "TAX", "LXA", "LDY", "LDA", "LDX", "LAX",
		"BCS", "LDA", "JAM", "LAX", "LDY", "LDA", "LDX", "LAX", "CLV", "LDA", "TSX", "LAS", "LDY", "LDA", "LDX", "LAX",
		"CPY", "CMP", "NOP", "DCP", "CPY", "CMP", "DEC", "DCP", "INY", "CMP", "DEX", "SBX", "CPY", "CMP", "DEC", "DCP",
		"BNE", "CMP", "JAM", "DCP", "NOP", "CMP", "DEC", "DCP", "CLD", "CMP", "NOP", "DCP", "NOP", "CMP", "DEC", "DCP",
		
		// "CPX", "SBC", "NOP", "ISC", "CPX", "SBC", "INC", "ISC", "INX", "SBC", "NOP", "USB", "CPX", "SBC", "INC", "ISC",
		"CPX", "SBC", "NOP", "ISB", "CPX", "SBC", "INC", "ISB", "INX", "SBC", "NOP", "SBC", "CPX", "SBC", "INC", "ISB", // match nestest
		// "BEQ", "SBC", "NOP", "ISC", "NOP", "SBC", "INC", "ISC", "SED", "SBC", "NOP", "ISC", "NOP", "SBC", "INC", "ISC",
		"BEQ", "SBC", "NOP", "ISB", "NOP", "SBC", "INC", "ISB", "SED", "SBC", "NOP", "ISB", "NOP", "SBC", "INC", "ISB", // match nestest
	};

	inline static const AddressingMode OPCODE_ADDRESSING_MAP[256] = {
	//  0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
		IMP, INX, IMP, INX, ZPG, ZPG, ZPG, ZPG, IMP, IMM, ACC, IMM, ABS, ABS, ABS, ABS, // 0
		REL, INY, IMP, INY, ZPX, ZPX, ZPX, ZPX, IMP, ABY, IMP, ABY, ABX, ABX, ABX, ABX, // 1
		ABS, INX, IMP, INX, ZPG, ZPG, ZPG, ZPG, IMP, IMM, ACC, IMM, ABS, ABS, ABS, ABS, // 2
		REL, INY, IMP, INY, ZPX, ZPX, ZPX, ZPX, IMP, ABY, IMP, ABY, ABX, ABX, ABX, ABX, // 3
		IMP, INX, IMP, INX, ZPG, ZPG, ZPG, ZPG, IMP, IMM, ACC, IMM, ABS, ABS, ABS, ABS, // 4
		REL, INY, IMP, INY, ZPX, ZPX, ZPX, ZPX, IMP, ABY, IMP, ABY, ABX, ABX, ABX, ABX, // 5
		IMP, INX, IMP, INX, ZPG, ZPG, ZPG, ZPG, IMP, IMM, ACC, IMM, IND, ABS, ABS, ABS, // 6
		REL, INY, IMP, INY, ZPX, ZPX, ZPX, ZPX, IMP, ABY, IMP, ABY, ABX, ABX, ABX, ABX, // 7
		IMM, INX, IMM, INX, ZPG, ZPG, ZPG, ZPG, IMP, IMM, IMP, IMM, ABS, ABS, ABS, ABS, // 8
		REL, INY, IMP, INY, ZPX, ZPX, ZPY, ZPY, IMP, ABY, IMP, ABY, ABX, ABX, ABX, ABX, // 9
		IMM, INX, IMM, INX, ZPG, ZPG, ZPG, ZPG, IMP, IMM, IMP, IMM, ABS, ABS, ABS, ABS, // A
		REL, INY, IMP, INY, ZPX, ZPX, ZPY, ZPY, IMP, ABY, IMP, ABY, ABX, ABX, ABY, ABY, // B
		IMM, INX, IMM, INX, ZPG, ZPG, ZPG, ZPG, IMP, IMM, IMP, IMM, ABS, ABS, ABS, ABS, // C
		REL, INY, IMP, INY, ZPX, ZPX, ZPX, ZPX, IMP, ABY, IMP, ABY, ABX, ABX, ABX, ABX, // D
		IMM, INX, IMM, INX, ZPG, ZPG, ZPG, ZPG, IMP, IMM, IMP, IMM, ABS, ABS, ABS, ABS, // E
		REL, INY, IMP, INY, ZPX, ZPX, ZPX, ZPX, IMP, ABY, IMP, ABY, ABX, ABX, ABX, ABX, // F
	};

	// MEMORY CONSTANTS

	static const uint16_t STACK_BASE = 0x0100;
	static const uint16_t NMI_VECTOR = 0xFFFA;
	static const uint16_t RESET_VECTOR = 0xFFFC;
	static const uint16_t IRQ_VECTOR = 0xFFFE;



	// REGISTERS
	uint8_t a;        // Accumulator
	uint8_t x;        // X Register
	uint8_t y;        // Y Register
	uint16_t pc;      // Program Counter
	uint8_t s;        // Stack Pointer
	StatusRegister p; // Status Register

	Bus* bus;

	// STATE

	long int cycles;
	bool enableCpuLog = false;

	bool pageCrossed;
	
	bool interruptDelay = false;
	

public:

	bool irqPending = false;
	
	bool jammed = false;

	CPU();
	void reset();
	void powerOn();

	// MEMORY INTERFACING

	uint8_t readMem(uint16_t addr);
	void writeMem(uint16_t addr, uint8_t val);
	uint16_t readMem16(uint16_t addr);
	void writeMem16(uint16_t addr, uint16_t val);
	uint16_t readMem16Wrap(uint16_t addr);

	uint8_t pull();
	void push(uint8_t val);
	uint16_t pull16();
	void push16(uint16_t val);


	// getters and setters

	bool isJammed();
	void setJammed(bool jammed);
	long int getCycles();
	void enableLogging(bool enable);

	void connectBus(Bus* busRef);
	void disconnectBus();

private:


	// helpers

	uint16_t getOperandAddress(AddressingMode mode, AccessType type);

	// specific helpers

	uint8_t _neg(uint8_t val);
	int8_t _toSigned(uint8_t val);
	uint8_t _toUnsigned(int8_t val);
	void _setZNFlags(uint8_t val);
	void _addToAccumulator(uint8_t val);
	void _compare(uint8_t val1, uint8_t val2);
	void _branch(bool condition);
	void _interrupt(InterruptVector vec);
	uint8_t _getStatus(bool flagB);
	void _setStatus(uint8_t status);




	// CPU INSTRUCTIONS

	// OFFICIAL OPCODES

	void op_ADC(AddressingMode mode);
	void op_AND(AddressingMode mode);
	void op_ASL(AddressingMode mode);
	void op_BCC(AddressingMode mode);
	void op_BCS(AddressingMode mode);
	void op_BEQ(AddressingMode mode);
	void op_BIT(AddressingMode mode);
	void op_BMI(AddressingMode mode);
	void op_BNE(AddressingMode mode);
	void op_BPL(AddressingMode mode);
	void op_BRK(AddressingMode mode);
	void op_BVC(AddressingMode mode);
	void op_BVS(AddressingMode mode);
	void op_CLC(AddressingMode mode);
	void op_CLD(AddressingMode mode);
	void op_CLI(AddressingMode mode);
	void op_CLV(AddressingMode mode);
	void op_CMP(AddressingMode mode);
	void op_CPX(AddressingMode mode);
	void op_CPY(AddressingMode mode);
	void op_DEC(AddressingMode mode);
	void op_DEX(AddressingMode mode);
	void op_DEY(AddressingMode mode);
	void op_EOR(AddressingMode mode);
	void op_INC(AddressingMode mode);
	void op_INX(AddressingMode mode);
	void op_INY(AddressingMode mode);
	void op_JMP(AddressingMode mode);
	void op_JSR(AddressingMode mode);
	void op_LDA(AddressingMode mode);
	void op_LDX(AddressingMode mode);
	void op_LDY(AddressingMode mode);
	void op_LSR(AddressingMode mode);
	void op_NOP(AddressingMode mode);
	void op_ORA(AddressingMode mode);
	void op_PHA(AddressingMode mode);
	void op_PHP(AddressingMode mode);
	void op_PLA(AddressingMode mode);
	void op_PLP(AddressingMode mode);
	void op_ROL(AddressingMode mode);
	void op_ROR(AddressingMode mode);
	void op_RTI(AddressingMode mode);
	void op_RTS(AddressingMode mode);
	void op_SBC(AddressingMode mode);
	void op_SEC(AddressingMode mode);
	void op_SED(AddressingMode mode);
	void op_SEI(AddressingMode mode);
	void op_STA(AddressingMode mode);
	void op_STX(AddressingMode mode);
	void op_STY(AddressingMode mode);
	void op_TAX(AddressingMode mode);
	void op_TAY(AddressingMode mode);
	void op_TSX(AddressingMode mode);
	void op_TXA(AddressingMode mode);
	void op_TXS(AddressingMode mode);
	void op_TYA(AddressingMode mode);

	// UNOFFICIAL OPCODES
	// names following https://www.masswerk.at/6502/6502_instruction_set.html

	void op_ALR(AddressingMode mode);
	void op_ANC(AddressingMode mode);
	void op_ANC2(AddressingMode mode);
	void op_ANE(AddressingMode mode);
	void op_ARR(AddressingMode mode);
	void op_DCP(AddressingMode mode);
	void op_ISC(AddressingMode mode);
	void op_LAS(AddressingMode mode);
	void op_LAX(AddressingMode mode);
	void op_LXA(AddressingMode mode);
	void op_RLA(AddressingMode mode);
	void op_RRA(AddressingMode mode);
	void op_SAX(AddressingMode mode);
	void op_SBX(AddressingMode mode);
	void op_SHA(AddressingMode mode);
	void op_SHX(AddressingMode mode);
	void op_SHY(AddressingMode mode);
	void op_SLO(AddressingMode mode);
	void op_SRE(AddressingMode mode);
	void op_TAS(AddressingMode mode);
	void op_USBC(AddressingMode mode);
	void op_JAM(AddressingMode mode);


public:

	// RUNNING

	void run();
	bool clock(); // bool to pass nmi
	void runInstruction(uint8_t opcode);

	// External interrupt trigger (called by PPU when NMI occurs)
	void triggerNMI();

	// Logging helper: write a single-line instruction trace to cpu.log when
	// `enableCpuLog` is true. Format example:
	// C000 4C F5 C5 a:00, x:00, y:00, p:24, sp:FD cyc:7
	void logInstruction(uint16_t instrPc, uint8_t opcode, const uint8_t* opcodeBytes, size_t byteCount, long int prev_cycles);

};
