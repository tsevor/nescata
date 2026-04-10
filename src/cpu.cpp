#include "cpu.hpp"
#include "bus.hpp"

// CPU IMPLEMENTATION

// FUNCTIONS

CPU::CPU() {}

uint8_t CPU::readMem(uint16_t addr) {
	if (bus) {
		return bus->read(addr);
	}
	return 0;
}

void CPU::writeMem(uint16_t addr, uint8_t val) {
	if (bus) {
		bus->write(addr, val);
	}
}

uint16_t CPU::readMem16(uint16_t addr) {
	return readMem(addr) | (readMem(addr + 1) << 8);
}

void CPU::writeMem16(uint16_t addr, uint16_t val) {
	writeMem(addr, val & 0xff);
	writeMem(addr + 1, (val >> 8) & 0xff);
}

uint16_t CPU::readMem16Wrap(uint16_t addr) {
	if ((addr & 0x00ff) == 0x00ff)
		return readMem(addr) | (readMem(addr & 0xff00) << 8);
	return readMem16(addr);
}

uint8_t CPU::pull() {
	s++;
	return readMem(STACK_BASE + s);
}

void CPU::push(uint8_t val) {
	writeMem(STACK_BASE + s, val);
	s--;
}

uint16_t CPU::pull16() {
	uint8_t low = pull();
	uint8_t high = pull();
	return (high << 8) | low;
}

void CPU::push16(uint16_t val) {
	push((val >> 8) & 0xff);
	push(val & 0xff);
}


// getters and setters

bool CPU::isJammed() {
	return jammed;
}

long int CPU::getCycles() {
	return cycles;
}

void CPU::enableLogging(bool enable) {
	enableCpuLog = enable;
}


void CPU::connectBus(Bus* busRef) {
	bus = busRef;
}

void CPU::disconnectBus() {
	bus = nullptr;
}

uint16_t CPU::getOperandAddress(AddressingMode mode) {
	uint16_t addr;

	switch (mode) {
		case IMP:
		case ACC:
			return 0; // No operand
		case IMM:
			return pc++;
		case ZPG:
			return readMem(pc++);
		case ZPX:
			return (readMem(pc++) + x) & 0xff;
		case ZPY:
			return (readMem(pc++) + y) & 0xff;
		case REL: {
			int8_t offset = readMem(pc++);
			return pc + offset;
		}
		case ABS:
			addr = readMem16(pc);
			pc += 2;
			return addr;
		case ABX: {
			uint16_t base = readMem16(pc);
			addr = base + x;
			pc += 2;
			pageCrossed = ((base & 0xFF00) != (addr & 0xFF00));
			return addr;
		}
		case ABY: {
			uint16_t base = readMem16(pc);
			addr = base + y;
			pc += 2;
			pageCrossed = ((base & 0xFF00) != (addr & 0xFF00));
			return addr;
		}
		case IND:
			// indirect JMP bug is emulated here
			addr = readMem16(pc);
			pc += 2;
			return readMem16Wrap(addr);
		case INX: {
			uint8_t zeroPageAddr = readMem(pc++);
			uint8_t effectiveAddr = (zeroPageAddr + x) & 0xFF;
			return readMem16Wrap(effectiveAddr);
		}
		case INY: {
			uint16_t base = readMem(pc++);
			uint16_t baseAddr = readMem16Wrap(base);
			addr = baseAddr + y;
			pageCrossed = ((baseAddr & 0xFF00) != (addr & 0xFF00));
			return addr;
		}
		default:
			return 0; // Should not happen
	}
}


uint8_t CPU::_getStatus(bool flagB) {
	if (flagB)
		return p.raw | 0b00110000;
	else
		return p.raw & 0b11101111 | 0b00100000;
}

void CPU::_setStatus(uint8_t status) {
	p.raw = status & 0b11101111 | 0b00100000;
}

// CPU INSTRUCTION HELPERS

uint8_t CPU::_neg(uint8_t val) {
	return ~val + 1;
}

int8_t CPU::_toSigned(uint8_t val) {
	if (val & 0x80)
		return -((~val + 1) & 0xFF);
	else
		return val;
}

uint8_t CPU::_toUnsigned(int8_t val) {
	if (val < 0)
		return (~(-val) + 1) & 0xFF;
	else
		return val & 0xFF;
}

void CPU::_setZNFlags(uint8_t val) {
	p.Z = (val == 0);
	p.N = (val & 0x80) != 0;
}

void CPU::_addToAccumulator(uint8_t val) {
	uint16_t sum = a + val + p.C;
	p.C = (sum > 0xFF);
	p.V = (~(a ^ val) & (a ^ sum) & 0x80) != 0;
	uint8_t result = sum & 0xFF;
	a = result;
	_setZNFlags(result);
}

void CPU::_compare(uint8_t val1, uint8_t val2) {
	uint16_t diff = static_cast<uint16_t>(val1) - static_cast<uint16_t>(val2);
	p.C = (val1 >= val2);
	uint8_t result = diff & 0xFF;
	_setZNFlags(result);
}

void CPU::_branch(bool condition) {
	int8_t offset = readMem(pc++);

	if (condition) {
		cycles++;
		const uint16_t target_addr = pc + offset;

		if ((pc & 0xFF00) != (target_addr & 0xFF00)) {
			cycles++;
		}

		pc = target_addr;
	}
}

void CPU::_interrupt(CPU::InterruptVector vec) {
	if (vec == CPU::VECTOR_RESET) {
		p.I = 1; // Disable IRQs
		pc = readMem16(RESET_VECTOR);
		return;
	}

	if (vec == CPU::VECTOR_IRQ && p.I) {
		return;
	}

	push16(pc);

	uint8_t flags_to_push = p.raw;

	if (vec == CPU::VECTOR_BRK) {
		flags_to_push |= (1 << 4);
	} else {
		flags_to_push &= ~(1 << 4);
	}

	flags_to_push |= (1 << 5);

	push(flags_to_push);

	p.I = 1;

	uint16_t vectorAddr;
	if (vec == CPU::VECTOR_NMI) {
		vectorAddr = NMI_VECTOR;
	} else {
		vectorAddr = IRQ_VECTOR;
	}

	pc = readMem16(vectorAddr);
}

// CPU INSTRUCTIONS

void CPU::op_ADC(AddressingMode mode) {
	uint16_t addr = getOperandAddress(mode);
	_addToAccumulator(readMem(addr));
	if (mode == ABX || mode == ABY || mode == INY) {
		if (pageCrossed) cycles++;
	}
}

void CPU::op_AND(AddressingMode mode) {
	uint16_t addr = getOperandAddress(mode);
	a &= readMem(addr);
	_setZNFlags(a);
	if (mode == ABX || mode == ABY || mode == INY) {
		if (pageCrossed) cycles++;
	}
}

void CPU::op_ASL(AddressingMode mode) {
	if (mode == ACC) {
		p.C = (a & 0x80) != 0;
		a <<= 1;
		_setZNFlags(a);
	} else {
		uint16_t addr = getOperandAddress(mode);
		uint8_t val = readMem(addr);
		p.C = (val & 0x80) != 0;
		val <<= 1;
		writeMem(addr, val);
		_setZNFlags(val);
	}
}

void CPU::op_BCC(AddressingMode mode) {
	_branch(p.C == 0);
}

void CPU::op_BCS(AddressingMode mode) {
	_branch(p.C == 1);
}

void CPU::op_BEQ(AddressingMode mode) {
	_branch(p.Z == 1);
}

void CPU::op_BIT(AddressingMode mode) {
	uint16_t addr = getOperandAddress(mode);
	uint8_t val = readMem(addr);
	p.Z = (a & val) == 0;
	p.N = (val & 0x80) != 0;
	p.V = (val & 0x40) != 0;
}

void CPU::op_BNE(AddressingMode mode) {
	_branch(p.Z == 0);
}

void CPU::op_BMI(AddressingMode mode) {
	_branch(p.N == 1);
}

void CPU::op_BPL(AddressingMode mode) {
	_branch(p.N == 0);
}

void CPU::op_BRK(AddressingMode mode) {
	pc++;
	_interrupt(VECTOR_BRK);
}

void CPU::op_BVS(AddressingMode mode) {
	_branch(p.V == 1);
}

void CPU::op_BVC(AddressingMode mode) {
	_branch(p.V == 0);
}

void CPU::op_CLC(AddressingMode mode) {
	p.C = 0;
}

void CPU::op_CLD(AddressingMode mode) {
	p.D = 0;
}

void CPU::op_CLI(AddressingMode mode) {
	p.I = 0;
}

void CPU::op_CLV(AddressingMode mode) {
	p.V = 0;
}

void CPU::op_CMP(AddressingMode mode) {
	uint16_t addr = getOperandAddress(mode);
	_compare(a, readMem(addr));
	if (mode == ABX || mode == ABY || mode == INY) {
		if (pageCrossed) cycles++;
	}
}

void CPU::op_CPX(AddressingMode mode) {
	uint16_t addr = getOperandAddress(mode);
	_compare(x, readMem(addr));
}

void CPU::op_CPY(AddressingMode mode) {
	uint16_t addr = getOperandAddress(mode);
	_compare(y, readMem(addr));
}

void CPU::op_DEC(AddressingMode mode) {
	uint16_t addr = getOperandAddress(mode);
	uint8_t val = readMem(addr) - 1;
	writeMem(addr, val);
	_setZNFlags(val);
}

void CPU::op_DEX(AddressingMode mode) {
	x--;
	_setZNFlags(x);
}

void CPU::op_DEY(AddressingMode mode) {
	y--;
	_setZNFlags(y);
}

void CPU::op_EOR(AddressingMode mode) {
	uint16_t addr = getOperandAddress(mode);
	a ^= readMem(addr);
	_setZNFlags(a);
	if (mode == ABX || mode == ABY || mode == INY) {
		if (pageCrossed) cycles++;
	}
}

void CPU::op_INC(AddressingMode mode) {
	uint16_t addr = getOperandAddress(mode);
	uint8_t val = readMem(addr) + 1;
	writeMem(addr, val);
	_setZNFlags(val);
}

void CPU::op_INX(AddressingMode mode) {
	x++;
	_setZNFlags(x);
}

void CPU::op_INY(AddressingMode mode) {
	y++;
	_setZNFlags(y);
}

void CPU::op_JMP(AddressingMode mode) {
	pc = getOperandAddress(mode);
}

void CPU::op_JSR(AddressingMode mode) {
	uint16_t addr = readMem16(pc);
	push16(pc + 1);
	pc = addr;
}

void CPU::op_LDA(AddressingMode mode) {
	uint16_t addr = getOperandAddress(mode);
	a = readMem(addr);
	_setZNFlags(a);
	if (mode == ABX || mode == ABY || mode == INY) {
		if (pageCrossed) cycles++;
	}
}

void CPU::op_LDX(AddressingMode mode) {
	uint16_t addr = getOperandAddress(mode);
	x = readMem(addr);
	_setZNFlags(x);
	if (mode == ABY || mode == INY) { // LDX uses Absolute,Y
		if (pageCrossed) cycles++;
	}
}

void CPU::op_LDY(AddressingMode mode) {
	uint16_t addr = getOperandAddress(mode);
	y = readMem(addr);
	_setZNFlags(y);
	if (mode == ABX) { // LDY uses Absolute,X
		if (pageCrossed) cycles++;
	}
}

void CPU::op_LSR(AddressingMode mode) {
	if (mode == ACC) {
		p.C = a & 1;
		a >>= 1;
		_setZNFlags(a);
	} else {
		uint16_t addr = getOperandAddress(mode);
		uint8_t val = readMem(addr);
		p.C = val & 1;
		val >>= 1;
		writeMem(addr, val);
		_setZNFlags(val);
	}
}

void CPU::op_NOP(AddressingMode mode) {
	if (mode != IMP && mode != ACC) {
		getOperandAddress(mode);

		if (mode == ABX) {
			if (pageCrossed) {
				cycles++;
			}
		}
	}
}

void CPU::op_ORA(AddressingMode mode) {
	uint16_t addr = getOperandAddress(mode);
	a |= readMem(addr);
	_setZNFlags(a);
	if (mode == ABX || mode == ABY || mode == INY) {
		if (pageCrossed) cycles++;
	}
}

void CPU::op_PHA(AddressingMode mode) {
	push(a);
}

void CPU::op_PHP(AddressingMode mode) {
	push(_getStatus(true));
}

void CPU::op_PLA(AddressingMode mode) {
	a = pull();
	_setZNFlags(a);
}

void CPU::op_PLP(AddressingMode mode) {
	_setStatus(pull());
	p.U = 1;
}

void CPU::op_ROL(AddressingMode mode) {
	uint8_t carry = p.C;
	if (mode == ACC) {
		p.C = (a & 0x80) != 0;
		a = (a << 1) | carry;
		_setZNFlags(a);
	} else {
		uint16_t addr = getOperandAddress(mode);
		uint8_t val = readMem(addr);
		p.C = (val & 0x80) != 0;
		val = (val << 1) | carry;
		writeMem(addr, val);
		_setZNFlags(val);
	}
}

void CPU::op_ROR(AddressingMode mode) {
	uint8_t carry = p.C;
	if (mode == ACC) {
		p.C = a & 1;
		a = (a >> 1) | (carry << 7);
		_setZNFlags(a);
	} else {
		uint16_t addr = getOperandAddress(mode);
		uint8_t val = readMem(addr);
		p.C = val & 1;
		val = (val >> 1) | (carry << 7);
		writeMem(addr, val);
		_setZNFlags(val);
	}
}

void CPU::op_RTI(AddressingMode mode) {
	_setStatus(pull());
	pc = pull16();
}

void CPU::op_RTS(AddressingMode mode) {
	pc = pull16() + 1;
}

void CPU::op_SBC(AddressingMode mode) {
	uint16_t addr = getOperandAddress(mode);
	_addToAccumulator(readMem(addr) ^ 0xFF);
	if (mode == ABX || mode == ABY || mode == INY) {
		if (pageCrossed) cycles++;
	}
}

void CPU::op_SEC(AddressingMode mode) {
	p.C = 1;
}

void CPU::op_SED(AddressingMode mode) {
	p.D = 1;
}

void CPU::op_SEI(AddressingMode mode) {
	p.I = 1;
}

void CPU::op_STA(AddressingMode mode) {
	writeMem(getOperandAddress(mode), a);
}

void CPU::op_STX(AddressingMode mode) {
	writeMem(getOperandAddress(mode), x);
}

void CPU::op_STY(AddressingMode mode) {
	writeMem(getOperandAddress(mode), y);
}

void CPU::op_TAX(AddressingMode mode) {
	x = a;
	_setZNFlags(x);
}

void CPU::op_TAY(AddressingMode mode) {
	y = a;
	_setZNFlags(y);
}

void CPU::op_TSX(AddressingMode mode) {
	x = s;
	_setZNFlags(x);
}

void CPU::op_TXA(AddressingMode mode) {
	a = x;
	_setZNFlags(a);
}

void CPU::op_TXS(AddressingMode mode) {
	s = x;
}

void CPU::op_TYA(AddressingMode mode) {
	a = y;
	_setZNFlags(a);
}

// UNOFFICIAL OPCODES

void CPU::op_ALR(AddressingMode mode) {
	uint16_t addr = getOperandAddress(mode);
	uint8_t val = readMem(addr);
	a &= val;
	p.C = (a & 0x01);
	a >>= 1;
	_setZNFlags(a);
}

void CPU::op_ANC(AddressingMode mode) {
	uint16_t addr = getOperandAddress(mode);
	a &= readMem(addr);
	_setZNFlags(a);
	p.C = p.N;
}

void CPU::op_ANC2(AddressingMode mode) {
	// Functionally identical to ANC
	uint16_t addr = getOperandAddress(mode);
	a &= readMem(addr);
	_setZNFlags(a);
	p.C = p.N;
}

void CPU::op_ANE(AddressingMode mode) {
	// Highly unstable, often treated as a NOP that fetches an operand
	getOperandAddress(mode);
}

void CPU::op_ARR(AddressingMode mode) {
	// This instruction has very peculiar flag behavior
	uint16_t addr = getOperandAddress(mode);
	a &= readMem(addr);
	uint8_t carry = p.C;
	a = (a >> 1) | (carry << 7);
	_setZNFlags(a);
	p.C = (a >> 6) & 1;
	p.V = ((a >> 6) & 1) ^ ((a >> 5) & 1);
}

void CPU::op_DCP(AddressingMode mode) {
	// DEC oper + CMP oper
	uint16_t addr = getOperandAddress(mode);
	uint8_t val = readMem(addr) - 1;
	writeMem(addr, val);
	_compare(a, val);
}

void CPU::op_ISC(AddressingMode mode) {
	// INC oper + SBC oper
	uint16_t addr = getOperandAddress(mode);
	uint8_t val = readMem(addr) + 1;
	writeMem(addr, val);
	_addToAccumulator(val ^ 0xFF);
}

void CPU::op_LAS(AddressingMode mode) {
	uint16_t addr = getOperandAddress(mode);
	uint8_t val = readMem(addr) & s;
	a = val;
	x = val;
	s = val;
	_setZNFlags(val);
	if (pageCrossed) cycles++;
}

void CPU::op_LAX(AddressingMode mode) {
	// LDA oper + LDX oper
	uint16_t addr = getOperandAddress(mode);
	uint8_t val = readMem(addr);
	a = val;
	x = val;
	_setZNFlags(val);
	if (pageCrossed) cycles++;
}

void CPU::op_LXA(AddressingMode mode) {
	// so unstable that it's not really worth implementing
	getOperandAddress(mode);
}

void CPU::op_RLA(AddressingMode mode) {
	// ROL oper + AND oper
	uint16_t addr = getOperandAddress(mode);
	uint8_t val = readMem(addr);
	uint8_t carry = p.C;
	p.C = (val & 0x80) != 0;
	val = (val << 1) | carry;
	writeMem(addr, val);
	a &= val;
	_setZNFlags(a);
}

void CPU::op_RRA(AddressingMode mode) {
	// ROR oper + ADC oper
	uint16_t addr = getOperandAddress(mode);
	uint8_t val = readMem(addr);
	uint8_t carry = p.C;
	p.C = (val & 0x01) != 0;
	val = (val >> 1) | (carry << 7);
	writeMem(addr, val);
	_addToAccumulator(val);
}

void CPU::op_SAX(AddressingMode mode) {
	uint16_t addr = getOperandAddress(mode);
	writeMem(addr, a & x);
}

void CPU::op_SBX(AddressingMode mode) {
	uint16_t addr = getOperandAddress(mode);
	uint8_t val = readMem(addr);
	uint16_t diff = (a & x) - val;
	p.C = (diff < 0x100);
	x = diff & 0xFF;
	_setZNFlags(x);
}

void CPU::op_SHA(AddressingMode mode) {
	uint16_t addr = getOperandAddress(mode);
	uint8_t highByte = (addr >> 8) + 1;
	uint8_t val = a & x & highByte;
	writeMem(addr, val);
}

void CPU::op_SHX(AddressingMode mode) {
	uint16_t addr = getOperandAddress(mode);
	uint8_t highByte = (addr >> 8) + 1;
	uint8_t val = x & highByte;
	writeMem(addr, val);
}

void CPU::op_SHY(AddressingMode mode) {
	uint16_t addr = getOperandAddress(mode);
	uint8_t highByte = (addr >> 8) + 1;
	uint8_t val = y & highByte;
	writeMem(addr, val);
}

void CPU::op_SLO(AddressingMode mode) {
	// ASL oper + ORA oper
	uint16_t addr = getOperandAddress(mode);
	uint8_t val = readMem(addr);
	p.C = (val & 0x80) != 0;
	val <<= 1;
	writeMem(addr, val);
	a |= val;
	_setZNFlags(a);
}

void CPU::op_SRE(AddressingMode mode) {
	// LSR oper + EOR oper
	uint16_t addr = getOperandAddress(mode);
	uint8_t val = readMem(addr);
	p.C = (val & 0x01) != 0;
	val >>= 1;
	writeMem(addr, val);
	a ^= val;
	_setZNFlags(a);
}

void CPU::op_TAS(AddressingMode mode) {
	// Unstable, often treated as a NOP that fetches an operand
	getOperandAddress(mode);
}

void CPU::op_USBC(AddressingMode mode) {
	// Same as official SBC, just an alternate opcode
	uint16_t addr = getOperandAddress(mode);
	_addToAccumulator(readMem(addr) ^ 0xFF);
}

void CPU::op_JAM(AddressingMode mode) {
	jammed = true;
}


// RUNNING

void CPU::reset() {
	s -= 3;
	p.I = 1;
	p.D = 0;
	pc = readMem16(RESET_VECTOR);
	cycles = 7;
	jammed = false;
}

void CPU::powerOn() {
	if (enableCpuLog) {
		FILE* f = fopen("cpu.log", "w");
		if (f) {
			fclose(f);
		}
	}

	a = 0;
	x = 0;
	y = 0;

	p.raw = 0x24;
	s = 0xFD;

	pc = readMem16(RESET_VECTOR);

	cycles = 7;
	jammed = false;
}

bool CPU::clock() {
	// if jammed, do nothing
	// return true to allow window to update
	if (jammed) {
		return true;
	}
	// Capture program counter at instruction start so log lines show the
	// correct address and bytes for the instruction executed.
	uint16_t instrPc = pc;
	uint8_t opcode = readMem(pc++);

	// Reset page cross flag for each new instruction
	pageCrossed = false;

	if (enableCpuLog) {
		// Read up to 3 operand bytes for logging (safe: don't advance pc here)
		uint8_t opcodeBytes[3] = {0, 0, 0};
		opcodeBytes[0] = readMem(instrPc);
		opcodeBytes[1] = readMem(instrPc + 1);
		opcodeBytes[2] = readMem(instrPc + 2);

		// Determine byte count from the addressing mode table
		auto mode = OPCODE_ADDRESSING_MAP[opcode];
		size_t byteCount = 1;
		switch (mode) {
			case IMM:
			case ZPG:
			case ZPX:
			case ZPY:
			case INX:
			case INY:
			case REL:
				byteCount = 2;
				break;
			case ABS:
			case ABX:
			case ABY:
			case IND:
				byteCount = 3;
				break;
			default:
				byteCount = 1;
		}
		logInstruction(instrPc, opcode, opcodeBytes, byteCount);
	}

	long int prev_cycles = cycles;
	// Add base cycles for this instruction
	cycles += OPCODE_CYCLES_MAP[opcode];

	runInstruction(opcode);
	int diff_cycles = cycles - prev_cycles;

	if (bus) {
		return bus->clock(diff_cycles * 12);
	}

	return false;
}

void CPU::triggerNMI() {
	_interrupt(VECTOR_NMI);
}

void CPU::logInstruction(uint16_t instrPc, uint8_t opcode, const uint8_t* opcodeBytes, size_t byteCount) {
	// Open cpu.log for append each time to keep implementation simple and
	// avoid holding a global file handle. This is fine for debugging but
	// could be optimized later.
	FILE* f = fopen("cpu.log", "a");
	if (!f) return;

	// Print PC
	fprintf(f, "%04X ", instrPc);

	// Print up to 3 bytes in hex (space-separated)
	for (size_t i = 0; i < 3; ++i) {
		if (i < byteCount) {
			fprintf(f, "%02X", opcodeBytes[i]);
		} else {
			fprintf(f, "  ");
		}
		if (i < 2) fprintf(f, " ");
	}

	fprintf(f, " ");
	// Print opcode mnemonic
	fprintf(f, "%-3s", OPCODE_MNEMONIC_MAP[opcode]);

	// Print CPU state
	char p_bits[9];
	for (int i = 7; i >= 0; --i) {
		p_bits[7 - i] = ((p.raw >> i) & 1) ? '1' : '0';
	}
	p_bits[8] = '\0';
	fprintf(f, " a:%02X x:%02X y:%02X p:%s sp:%02X cyc:%ld\n",
			a, x, y, p_bits, s, cycles);

	fclose(f);
}

void CPU::runInstruction(uint8_t opcode) {
	switch (opcode) {
		// ---------   OPCODES   --------- //     OPCODE | BYTES | CYCLES | ADDRESSING
		case 0x00: op_BRK(IMP); break; // BRK (0x00) | 1 | 7  | implied
		case 0x01: op_ORA(INX); break; // ORA (0x01) | 2 | 6  | (indirect,X)
		case 0x02: op_JAM(IMP); break; // JAM (0x02) | 1 | 0  | implied
		case 0x03: op_SLO(INX); break; // SLO (0x03) | 2 | 8  | (indirect,X)
		case 0x04: op_NOP(ZPG); break; // NOP (0x04) | 2 | 3  | zeropage
		case 0x05: op_ORA(ZPG); break; // ORA (0x05) | 2 | 3  | zeropage
		case 0x06: op_ASL(ZPG); break; // ASL (0x06) | 2 | 5  | zeropage
		case 0x07: op_SLO(ZPG); break; // SLO (0x07) | 2 | 5  | zeropage
		case 0x08: op_PHP(IMP); break; // PHP (0x08) | 1 | 3  | implied
		case 0x09: op_ORA(IMM); break; // ORA (0x09) | 2 | 2  | immediate
		case 0x0a: op_ASL(ACC); break; // ASL (0x0A) | 1 | 2  | accumulator
		case 0x0b: op_ANC(IMM); break; // ANC (0x0B) | 2 | 2  | immediate
		case 0x0c: op_NOP(ABS); break; // NOP (0x0C) | 3 | 4  | absolute
		case 0x0d: op_ORA(ABS); break; // ORA (0x0D) | 3 | 4  | absolute
		case 0x0e: op_ASL(ABS); break; // ASL (0x0E) | 3 | 6  | absolute
		case 0x0f: op_SLO(ABS); break; // SLO (0x0F) | 3 | 6  | absolute

		case 0x10: op_BPL(REL); break; // BPL (0x10) | 2 | 2**| relative
		case 0x11: op_ORA(INY); break; // ORA (0x11) | 2 | 5* | (indirect),Y
		case 0x12: op_JAM(IMP); break; // JAM (0x12) | 1 | 0  | implied
		case 0x13: op_SLO(INY); break; // SLO (0x13) | 2 | 8  | (indirect),Y
		case 0x14: op_NOP(ZPX); break; // NOP (0x14) | 2 | 4  | zeropage,X
		case 0x15: op_ORA(ZPX); break; // ORA (0x15) | 2 | 4  | zeropage,X
		case 0x16: op_ASL(ZPX); break; // ASL (0x16) | 2 | 6  | zeropage,X
		case 0x17: op_SLO(ZPX); break; // SLO (0x17) | 2 | 6  | zeropage,X
		case 0x18: op_CLC(IMP); break; // CLC (0x18) | 1 | 2  | implied
		case 0x19: op_ORA(ABY); break; // ORA (0x19) | 3 | 4* | absolute,Y
		case 0x1a: op_NOP(IMP); break; // NOP (0x1A) | 1 | 2  | implied
		case 0x1b: op_SLO(ABY); break; // SLO (0x1B) | 3 | 7  | absolute,Y
		case 0x1c: op_NOP(ABX); break; // NOP (0x1C) | 3 | 4* | absolute,X
		case 0x1d: op_ORA(ABX); break; // ORA (0x1D) | 3 | 4* | absolute,X
		case 0x1e: op_ASL(ABX); break; // ASL (0x1E) | 3 | 7  | absolute,X
		case 0x1f: op_SLO(ABX); break; // SLO (0x1F) | 3 | 7  | absolute,X

		case 0x20: op_JSR(ABS); break; // JSR (0x20) | 3 | 6  | absolute
		case 0x21: op_AND(INX); break; // AND (0x21) | 2 | 6  | (indirect,X)
		case 0x22: op_JAM(IMP); break; // JAM (0x22) | 1 | 0  | implied
		case 0x23: op_RLA(INX); break; // RLA (0x23) | 2 | 8  | (indirect,X)
		case 0x24: op_BIT(ZPG); break; // BIT (0x24) | 2 | 3  | zeropage
		case 0x25: op_AND(ZPG); break; // AND (0x25) | 2 | 3  | zeropage
		case 0x26: op_ROL(ZPG); break; // ROL (0x26) | 2 | 5  | zeropage
		case 0x27: op_RLA(ZPG); break; // RLA (0x27) | 2 | 5  | zeropage
		case 0x28: op_PLP(IMP); break; // PLP (0x28) | 1 | 4  | implied
		case 0x29: op_AND(IMM); break; // AND (0x29) | 2 | 2  | immediate
		case 0x2a: op_ROL(ACC); break; // ROL (0x2A) | 1 | 2  | accumulator
		case 0x2b: op_ANC2(IMM);break; // ANC (0x2B) | 2 | 2  | immediate
		case 0x2c: op_BIT(ABS); break; // BIT (0x2C) | 3 | 4  | absolute
		case 0x2d: op_AND(ABS); break; // AND (0x2D) | 3 | 4  | absolute
		case 0x2e: op_ROL(ABS); break; // ROL (0x2E) | 3 | 6  | absolute
		case 0x2f: op_RLA(ABS); break; // RLA (0x2F) | 3 | 6  | absolute

		case 0x30: op_BMI(REL); break; // BMI (0x30) | 2 | 2**| relative
		case 0x31: op_AND(INY); break; // AND (0x31) | 2 | 5* | (indirect),Y
		case 0x32: op_JAM(IMP); break; // JAM (0x32) | 1 | 0  | implied
		case 0x33: op_RLA(INY); break; // RLA (0x33) | 2 | 8  | (indirect),Y
		case 0x34: op_NOP(ZPX); break; // NOP (0x34) | 2 | 4  | zeropage,X
		case 0x35: op_AND(ZPX); break; // AND (0x35) | 2 | 4  | zeropage,X
		case 0x36: op_ROL(ZPX); break; // ROL (0x36) | 2 | 6  | zeropage,X
		case 0x37: op_RLA(ZPX); break; // RLA (0x37) | 2 | 6  | zeropage,X
		case 0x38: op_SEC(IMP); break; // SEC (0x38) | 1 | 2  | implied
		case 0x39: op_AND(ABY); break; // AND (0x39) | 3 | 4* | absolute,Y
		case 0x3a: op_NOP(IMP); break; // NOP (0x3A) | 1 | 2  | implied
		case 0x3b: op_RLA(ABY); break; // RLA (0x3B) | 3 | 7  | absolute,Y
		case 0x3c: op_NOP(ABX); break; // NOP (0x3C) | 3 | 4* | absolute,X
		case 0x3d: op_AND(ABX); break; // AND (0x3D) | 3 | 4* | absolute,X
		case 0x3e: op_ROL(ABX); break; // ROL (0x3E) | 3 | 7  | absolute,X
		case 0x3f: op_RLA(ABX); break; // RLA (0x3F) | 3 | 7  | absolute,X

		case 0x40: op_RTI(IMP); break; // RTI (0x40) | 1 | 6  | implied
		case 0x41: op_EOR(INX); break; // EOR (0x41) | 2 | 6  | (indirect,X)
		case 0x42: op_JAM(IMP); break; // JAM (0x42) | 1 | 0  | implied
		case 0x43: op_SRE(INX); break; // SRE (0x43) | 2 | 8  | (indirect,X)
		case 0x44: op_NOP(ZPG); break; // NOP (0x44) | 2 | 3  | zeropage
		case 0x45: op_EOR(ZPG); break; // EOR (0x45) | 2 | 3  | zeropage
		case 0x46: op_LSR(ZPG); break; // LSR (0x46) | 2 | 5  | zeropage
		case 0x47: op_SRE(ZPG); break; // SRE (0x47) | 2 | 5  | zeropage
		case 0x48: op_PHA(IMP); break; // PHA (0x48) | 1 | 3  | implied
		case 0x49: op_EOR(IMM); break; // EOR (0x49) | 2 | 2  | immediate
		case 0x4a: op_LSR(ACC); break; // LSR (0x4A) | 1 | 2  | accumulator
		case 0x4b: op_ALR(IMM); break; // ALR (0x4B) | 2 | 2  | immediate
		case 0x4c: op_JMP(ABS); break; // JMP (0x4C) | 3 | 3  | absolute
		case 0x4d: op_EOR(ABS); break; // EOR (0x4D) | 3 | 4  | absolute
		case 0x4e: op_LSR(ABS); break; // LSR (0x4E) | 3 | 6  | absolute
		case 0x4f: op_SRE(ABS); break; // SRE (0x4F) | 3 | 6  | absolute

		case 0x50: op_BVC(REL); break; // BVC (0x50) | 2 | 2**| relative
		case 0x51: op_EOR(INY); break; // EOR (0x51) | 2 | 5* | (indirect),Y
		case 0x52: op_JAM(IMP); break; // JAM (0x52) | 1 | 0  | implied
		case 0x53: op_SRE(INY); break; // SRE (0x53) | 2 | 8  | (indirect),Y
		case 0x54: op_NOP(ZPX); break; // NOP (0x54) | 2 | 4  | zeropage,X
		case 0x55: op_EOR(ZPX); break; // EOR (0x55) | 2 | 4  | zeropage,X
		case 0x56: op_LSR(ZPX); break; // LSR (0x56) | 2 | 6  | zeropage,X
		case 0x57: op_SRE(ZPX); break; // SRE (0x57) | 2 | 6  | zeropage,X
		case 0x58: op_CLI(IMP); break; // CLI (0x58) | 1 | 2  | implied
		case 0x59: op_EOR(ABY); break; // EOR (0x59) | 3 | 4* | absolute,Y
		case 0x5a: op_NOP(IMP); break; // NOP (0x5A) | 1 | 2  | implied
		case 0x5b: op_SRE(ABY); break; // SRE (0x5B) | 3 | 7  | absolute,Y
		case 0x5c: op_NOP(ABX); break; // NOP (0x5C) | 3 | 4* | absolute,X
		case 0x5d: op_EOR(ABX); break; // EOR (0x5D) | 3 | 4* | absolute,X
		case 0x5e: op_LSR(ABX); break; // LSR (0x5E) | 3 | 7  | absolute,X
		case 0x5f: op_SRE(ABX); break; // SRE (0x5F) | 3 | 7  | absolute,X

		case 0x60: op_RTS(IMP); break; // RTS (0x60) | 1 | 6  | implied
		case 0x61: op_ADC(INX); break; // ADC (0x61) | 2 | 6  | (indirect,X)
		case 0x62: op_JAM(IMP); break; // JAM (0x62) | 1 | 0  | implied
		case 0x63: op_RRA(INX); break; // RRA (0x63) | 2 | 8  | (indirect,X)
		case 0x64: op_NOP(ZPG); break; // NOP (0x64) | 2 | 3  | zeropage
		case 0x65: op_ADC(ZPG); break; // ADC (0x65) | 2 | 3  | zeropage
		case 0x66: op_ROR(ZPG); break; // ROR (0x66) | 2 | 5  | zeropage
		case 0x67: op_RRA(ZPG); break; // RRA (0x67) | 2 | 5  | zeropage
		case 0x68: op_PLA(IMP); break; // PLA (0x68) | 1 | 4  | implied
		case 0x69: op_ADC(IMM); break; // ADC (0x69) | 2 | 2  | immediate
		case 0x6a: op_ROR(ACC); break; // ROR (0x6A) | 1 | 2  | accumulator
		case 0x6b: op_ARR(IMM); break; // ARR (0x6B) | 2 | 2  | immediate
		case 0x6c: op_JMP(IND); break; // JMP (0x6C) | 3 | 5  | indirect
		case 0x6d: op_ADC(ABS); break; // ADC (0x6D) | 3 | 4  | absolute
		case 0x6e: op_ROR(ABS); break; // ROR (0x6E) | 3 | 6  | absolute
		case 0x6f: op_RRA(ABS); break; // RRA (0x6F) | 3 | 6  | absolute

		case 0x70: op_BVS(REL); break; // BVS (0x70) | 2 | 2**| relative
		case 0x71: op_ADC(INY); break; // ADC (0x71) | 2 | 5* | (indirect),Y
		case 0x72: op_JAM(IMP); break; // JAM (0x72) | 1 | 0  | implied
		case 0x73: op_RRA(INY); break; // RRA (0x73) | 2 | 8  | (indirect),Y
		case 0x74: op_NOP(ZPX); break; // NOP (0x74) | 2 | 4  | zeropage,X
		case 0x75: op_ADC(ZPX); break; // ADC (0x75) | 2 | 4  | zeropage,X
		case 0x76: op_ROR(ZPX); break; // ROR (0x76) | 2 | 6  | zeropage,X
		case 0x77: op_RRA(ZPX); break; // RRA (0x77) | 2 | 6  | zeropage,X
		case 0x78: op_SEI(IMP); break; // SEI (0x78) | 1 | 2  | implied
		case 0x79: op_ADC(ABY); break; // ADC (0x79) | 3 | 4* | absolute,Y
		case 0x7a: op_NOP(IMP); break; // NOP (0x7A) | 1 | 2  | implied
		case 0x7b: op_RRA(ABY); break; // RRA (0x7B) | 3 | 7  | absolute,Y
		case 0x7c: op_NOP(ABX); break; // NOP (0x7C) | 3 | 4* | absolute,X
		case 0x7d: op_ADC(ABX); break; // ADC (0x7D) | 3 | 4* | absolute,X
		case 0x7e: op_ROR(ABX); break; // ROR (0x7E) | 3 | 7  | absolute,X
		case 0x7f: op_RRA(ABX); break; // RRA (0x7F) | 3 | 7  | absolute,X

		case 0x80: op_NOP(IMM); break; // NOP (0x80) | 2 | 2  | immediate
		case 0x81: op_STA(INX); break; // STA (0x81) | 2 | 6  | (indirect,X)
		case 0x82: op_NOP(IMM); break; // NOP (0x82) | 2 | 2  | immediate
		case 0x83: op_SAX(INX); break; // SAX (0x83) | 2 | 6  | (indirect,X)
		case 0x84: op_STY(ZPG); break; // STY (0x84) | 2 | 3  | zeropage
		case 0x85: op_STA(ZPG); break; // STA (0x85) | 2 | 3  | zeropage
		case 0x86: op_STX(ZPG); break; // STX (0x86) | 2 | 3  | zeropage
		case 0x87: op_SAX(ZPG); break; // SAX (0x87) | 2 | 3  | zeropage
		case 0x88: op_DEY(IMP); break; // DEY (0x88) | 1 | 2  | implied
		case 0x89: op_NOP(IMM); break; // NOP (0x89) | 2 | 2  | immediate
		case 0x8a: op_TXA(IMP); break; // TXA (0x8A) | 1 | 2  | implied
		case 0x8b: op_ANE(IMM); break; // ANE (0x8B) | 2 | 2  | immediate (unstable)
		case 0x8c: op_STY(ABS); break; // STY (0x8C) | 3 | 4  | absolute
		case 0x8d: op_STA(ABS); break; // STA (0x8D) | 3 | 4  | absolute
		case 0x8e: op_STX(ABS); break; // STX (0x8E) | 3 | 4  | absolute
		case 0x8f: op_SAX(ABS); break; // SAX (0x8F) | 3 | 4  | absolute

		case 0x90: op_BCC(REL); break; // BCC (0x90) | 2 | 2**| relative
		case 0x91: op_STA(INY); break; // STA (0x91) | 2 | 6  | (indirect),Y
		case 0x92: op_JAM(IMP); break; // JAM (0x92) | 1 | 0  | implied
		case 0x93: op_SHA(INY); break; // SHA (0x93) | 2 | 6  | (indirect),Y
		case 0x94: op_STY(ZPX); break; // STY (0x94) | 2 | 4  | zeropage,X
		case 0x95: op_STA(ZPX); break; // STA (0x95) | 2 | 4  | zeropage,X
		case 0x96: op_STX(ZPY); break; // STX (0x96) | 2 | 4  | zeropage,Y
		case 0x97: op_SAX(ZPY); break; // SAX (0x97) | 2 | 4  | zeropage,Y
		case 0x98: op_TYA(IMP); break; // TYA (0x98) | 1 | 2  | implied
		case 0x99: op_STA(ABY); break; // STA (0x99) | 3 | 5  | absolute,Y
		case 0x9a: op_TXS(IMP); break; // TXS (0x9A) | 1 | 2  | implied
		case 0x9b: op_TAS(ABY); break; // TAS (0x9B) | 3 | 5  | absolute,Y
		case 0x9c: op_SHY(ABX); break; // SHY (0x9C) | 3 | 5  | absolute,X
		case 0x9d: op_STA(ABX); break; // STA (0x9D) | 3 | 5  | absolute,X
		case 0x9e: op_SHX(ABY); break; // SHX (0x9E) | 3 | 5  | absolute,Y
		case 0x9f: op_SHA(ABY); break; // SHA (0x9F) | 3 | 5  | absolute,Y

		case 0xa0: op_LDY(IMM); break; // LDY (0xA0) | 2 | 2  | immediate
		case 0xa1: op_LDA(INX); break; // LDA (0xA1) | 2 | 6  | (indirect,X)
		case 0xa2: op_LDX(IMM); break; // LDX (0xA2) | 2 | 2  | immediate
		case 0xa3: op_LAX(INX); break; // LAX (0xA3) | 2 | 6  | (indirect,X)
		case 0xa4: op_LDY(ZPG); break; // LDY (0xA4) | 2 | 3  | zeropage
		case 0xa5: op_LDA(ZPG); break; // LDA (0xA5) | 2 | 3  | zeropage
		case 0xa6: op_LDX(ZPG); break; // LDX (0xA6) | 2 | 3  | zeropage
		case 0xa7: op_LAX(ZPG); break; // LAX (0xA7) | 2 | 3  | zeropage
		case 0xa8: op_TAY(IMP); break; // TAY (0xA8) | 1 | 2  | implied
		case 0xa9: op_LDA(IMM); break; // LDA (0xA9) | 2 | 2  | immediate
		case 0xaa: op_TAX(IMP); break; // TAX (0xAA) | 1 | 2  | implied
		case 0xab: op_LXA(IMM); break; // LXA (0xAB) | 2 | 2  | immediate (unstable)
		case 0xac: op_LDY(ABS); break; // LDY (0xAC) | 3 | 4  | absolute
		case 0xad: op_LDA(ABS); break; // LDA (0xAD) | 3 | 4  | absolute
		case 0xae: op_LDX(ABS); break; // LDX (0xAE) | 3 | 4  | absolute
		case 0xaf: op_LAX(ABS); break; // LAX (0xAF) | 3 | 4  | absolute

		case 0xb0: op_BCS(REL); break; // BCS (0xB0) | 2 | 2**| relative
		case 0xb1: op_LDA(INY); break; // LDA (0xB1) | 2 | 5* | (indirect),Y
		case 0xb2: op_JAM(IMP); break; // JAM (0xB2) | 1 | 0  | implied
		case 0xb3: op_LAX(INY); break; // LAX (0xB3) | 2 | 5* | (indirect),Y
		case 0xb4: op_LDY(ZPX); break; // LDY (0xB4) | 2 | 4  | zeropage,X
		case 0xb5: op_LDA(ZPX); break; // LDA (0xB5) | 2 | 4  | zeropage,X
		case 0xb6: op_LDX(ZPY); break; // LDX (0xB6) | 2 | 4  | zeropage,Y
		case 0xb7: op_LAX(ZPY); break; // LAX (0xB7) | 2 | 4  | zeropage,Y
		case 0xb8: op_CLV(IMP); break; // CLV (0xB8) | 1 | 2  | implied
		case 0xb9: op_LDA(ABY); break; // LDA (0xB9) | 3 | 4* | absolute,Y
		case 0xba: op_TSX(IMP); break; // TSX (0xBA) | 1 | 2  | implied
		case 0xbb: op_LAS(ABY); break; // LAS (0xBB) | 3 | 4* | absolute,Y
		case 0xbc: op_LDY(ABX); break; // LDY (0xBC) | 3 | 4* | absolute,X
		case 0xbd: op_LDA(ABX); break; // LDA (0xBD) | 3 | 4* | absolute,X
		case 0xbe: op_LDX(ABY); break; // LDX (0xBE) | 3 | 4* | absolute,Y
		case 0xbf: op_LAX(ABY); break; // LAX (0xBF) | 3 | 4* | absolute,Y

		case 0xc0: op_CPY(IMM); break; // CPY (0xC0) | 2 | 2  | immediate
		case 0xc1: op_CMP(INX); break; // CMP (0xC1) | 2 | 6  | (indirect,X)
		case 0xc2: op_NOP(IMM); break; // NOP (0xC2) | 2 | 2  | immediate
		case 0xc3: op_DCP(INX); break; // DCP (0xC3) | 2 | 8  | (indirect,X)
		case 0xc4: op_CPY(ZPG); break; // CPY (0xC4) | 2 | 3  | zeropage
		case 0xc5: op_CMP(ZPG); break; // CMP (0xC5) | 2 | 3  | zeropage
		case 0xc6: op_DEC(ZPG); break; // DEC (0xC6) | 2 | 5  | zeropage
		case 0xc7: op_DCP(ZPG); break; // DCP (0xC7) | 2 | 5  | zeropage
		case 0xc8: op_INY(IMP); break; // INY (0xC8) | 1 | 2  | implied
		case 0xc9: op_CMP(IMM); break; // CMP (0xC9) | 2 | 2  | immediate
		case 0xca: op_DEX(IMP); break; // DEX (0xCA) | 1 | 2  | implied
		case 0xcb: op_SBX(IMM); break; // SBX (0xCB) | 2 | 2  | immediate
		case 0xcc: op_CPY(ABS); break; // CPY (0xCC) | 3 | 4  | absolute
		case 0xcd: op_CMP(ABS); break; // CMP (0xCD) | 3 | 4  | absolute
		case 0xce: op_DEC(ABS); break; // DEC (0xCE) | 3 | 6  | absolute
		case 0xcf: op_DCP(ABS); break; // DCP (0xCF) | 3 | 6  | absolute

		case 0xd0: op_BNE(REL); break; // BNE (0xD0) | 2 | 2**| relative
		case 0xd1: op_CMP(INY); break; // CMP (0xD1) | 2 | 5* | (indirect),Y
		case 0xd2: op_JAM(IMP); break; // JAM (0xD2) | 1 | 0  | implied
		case 0xd3: op_DCP(INY); break; // DCP (0xD3) | 2 | 8  | (indirect),Y
		case 0xd4: op_NOP(ZPX); break; // NOP (0xD4) | 2 | 4  | zeropage,X
		case 0xd5: op_CMP(ZPX); break; // CMP (0xD5) | 2 | 4  | zeropage,X
		case 0xd6: op_DEC(ZPX); break; // DEC (0xD6) | 2 | 6  | zeropage,X
		case 0xd7: op_DCP(ZPX); break; // DCP (0xD7) | 2 | 6  | zeropage,X
		case 0xd8: op_CLD(IMP); break; // CLD (0xD8) | 1 | 2  | implied
		case 0xd9: op_CMP(ABY); break; // CMP (0xD9) | 3 | 4* | absolute,Y
		case 0xda: op_NOP(IMP); break; // NOP (0xDA) | 1 | 2  | implied
		case 0xdb: op_DCP(ABY); break; // DCP (0xDB) | 3 | 7  | absolute,Y
		case 0xdc: op_NOP(ABX); break; // NOP (0xDC) | 3 | 4* | absolute,X
		case 0xdd: op_CMP(ABX); break; // CMP (0xDD) | 3 | 4* | absolute,X
		case 0xde: op_DEC(ABX); break; // DEC (0xDE) | 3 | 7  | absolute,X
		case 0xdf: op_DCP(ABX); break; // DCP (0xDF) | 3 | 7  | absolute,X

		case 0xe0: op_CPX(IMM); break; // CPX (0xE0) | 2 | 2  | immediate
		case 0xe1: op_SBC(INX); break; // SBC (0xE1) | 2 | 6  | (indirect,X)
		case 0xe2: op_NOP(IMM); break; // NOP (0xE2) | 2 | 2  | immediate
		case 0xe3: op_ISC(INX); break; // ISC (0xE3) | 2 | 8  | (indirect,X)
		case 0xe4: op_CPX(ZPG); break; // CPX (0xE4) | 2 | 3  | zeropage
		case 0xe5: op_SBC(ZPG); break; // SBC (0xE5) | 2 | 3  | zeropage
		case 0xe6: op_INC(ZPG); break; // INC (0xE6) | 2 | 5  | zeropage
		case 0xe7: op_ISC(ZPG); break; // ISC (0xE7) | 2 | 5  | zeropage
		case 0xe8: op_INX(IMP); break; // INX (0xE8) | 1 | 2  | implied
		case 0xe9: op_SBC(IMM); break; // SBC (0xE9) | 2 | 2  | immediate
		case 0xea: op_NOP(IMP); break; // NOP (0xEA) | 1 | 2  | implied
		case 0xeb: op_USBC(IMM);break; // USBC (0xEB)| 2 | 2  | immediate (SBC variant)
		case 0xec: op_CPX(ABS); break; // CPX (0xEC) | 3 | 4  | absolute
		case 0xed: op_SBC(ABS); break; // SBC (0xED) | 3 | 4  | absolute
		case 0xee: op_INC(ABS); break; // INC (0xEE) | 3 | 6  | absolute
		case 0xef: op_ISC(ABS); break; // ISC (0xEF) | 3 | 6  | absolute

		case 0xf0: op_BEQ(REL); break; // BEQ (0xF0) | 2 | 2**| relative
		case 0xf1: op_SBC(INY); break; // SBC (0xF1) | 2 | 5* | (indirect),Y
		case 0xf2: op_NOP(IMP); break; // NOP (0xF2) | 1 | 0  | implied
		case 0xf3: op_ISC(INY); break; // ISC (0xF3) | 2 | 8  | (indirect),Y
		case 0xf4: op_NOP(ZPX); break; // NOP (0xF4) | 2 | 4  | zeropage,X
		case 0xf5: op_SBC(ZPX); break; // SBC (0xF5) | 2 | 4  | zeropage,X
		case 0xf6: op_INC(ZPX); break; // INC (0xF6) | 2 | 6  | zeropage,X
		case 0xf7: op_ISC(ZPX); break; // ISC (0xF7) | 2 | 6  | zeropage,X
		case 0xf8: op_SED(IMP); break; // SED (0xF8) | 1 | 2  | implied
		case 0xf9: op_SBC(ABY); break; // SBC (0xF9) | 3 | 4* | absolute,Y
		case 0xfa: op_NOP(IMP); break; // NOP (0xFA) | 1 | 2  | implied
		case 0xfb: op_ISC(ABY); break; // ISC (0xFB) | 3 | 7  | absolute,Y
		case 0xfc: op_NOP(ABX); break; // NOP (0xFC) | 3 | 4* | absolute,X
		case 0xfd: op_SBC(ABX); break; // SBC (0xFD) | 3 | 4* | absolute,X
		case 0xfe: op_INC(ABX); break; // INC (0xFE) | 3 | 7  | absolute,X
		case 0xff: op_ISC(ABX); break; // ISC (0xFF) | 3 | 7  | absolute,X

		default:
			std::cout << "Unknown opcode: " << std::hex << (int)opcode << std::dec << "\n";
			break;
	}
}
