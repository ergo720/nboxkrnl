/*
* ergo720                Copyright (c) 2025
*/

#include "hw_exp.hpp"

#define KTRAP_FRAME_REG_OFFSET(field) (reinterpret_cast<ULONG>(&(reinterpret_cast<KTRAP_FRAME *>(0))->field))
#define GETREG(frame, reg) (reinterpret_cast<PULONG>((reinterpret_cast<ULONG>(frame)) + (reg)))[0]


static const UCHAR RM32[] = {
	KTRAP_FRAME_REG_OFFSET(Eax),
	KTRAP_FRAME_REG_OFFSET(Ecx),
	KTRAP_FRAME_REG_OFFSET(Edx),
	KTRAP_FRAME_REG_OFFSET(Ebx),
	KTRAP_FRAME_REG_OFFSET(TempEsp),
	KTRAP_FRAME_REG_OFFSET(Ebp),
	KTRAP_FRAME_REG_OFFSET(Esi),
	KTRAP_FRAME_REG_OFFSET(Edi)
};

static const UCHAR RM8[] = {
	KTRAP_FRAME_REG_OFFSET(Eax),     // al
	KTRAP_FRAME_REG_OFFSET(Ecx),     // cl
	KTRAP_FRAME_REG_OFFSET(Edx),     // dl
	KTRAP_FRAME_REG_OFFSET(Ebx),     // bl
	KTRAP_FRAME_REG_OFFSET(Eax) + 1, // ah
	KTRAP_FRAME_REG_OFFSET(Ecx) + 1, // ch
	KTRAP_FRAME_REG_OFFSET(Edx) + 1, // dh
	KTRAP_FRAME_REG_OFFSET(Ebx) + 1  // bh
};

NTSTATUS XBOXAPI KiDecodeDivisionOperand(PKTRAP_FRAME TrapFrame, ULONG HwEsp)
{
	NTSTATUS Status = STATUS_INTEGER_DIVIDE_BY_ZERO;
	HwEsp += 72; // sum of adjustments done by cpu and CREATE_KTRAP_FRAME_NO_CODE (assumes no ring switch when exception is triggered)

	__try {
		PBYTE CurrAddr = reinterpret_cast<PBYTE>(TrapFrame->Eip);
		BYTE InstrByte;
		BOOLEAN OperandSize = 32;
		bool LoopDone = false;

		while (!LoopDone) {
			InstrByte = *CurrAddr;

			switch (InstrByte)
			{
			case 0xF0: // lock
			case 0xF2: // repne, repnz
			case 0xF3: // rep, repe, repz
				break;

			case 0x2E: // cs override
			case 0x36: // ss override
			case 0x3E: // ds override
			case 0x26: // es override
			case 0x64: // fs override
			case 0x65: // gs override
				// We ignore the base of the segment, because xbox should always run with a flat memory space, where the base address
				// of all segments is zero
				break;

			case 0x66: // operand size override
				OperandSize = 16;
				break;

			case 0x67: // address size override
				// division with 16 bit address is ignored
				return Status;

			default:
				// Done with processing prefix bytes
				LoopDone = true;
			}

			++CurrAddr;
		}

		if ((InstrByte != 0xF6) && (InstrByte != 0xF7)) { // must be a DIV or IDIV instruction
			return Status;
		}

		if (InstrByte == 0xF6) {
			OperandSize = 8;
		}

		ULONG Divisor = 0;
		BYTE ModRm = *CurrAddr;
		BYTE Mod = (ModRm & 0xC0) >> 6;
		BYTE Reg = (ModRm & 0x38) >> 3;
		BYTE Rm = ModRm & 7;

		if ((Rm == 4) && (Mod != 3)) {
			// SIB byte is present after ModRm
			BYTE Sib = *(++CurrAddr);
			BYTE Scale = (Sib & 0xC0) >> 6;
			BYTE Index = (Sib & 0x38) >> 3;
			BYTE Base = Sib & 7;
			ULONG RegBase = GETREG(TrapFrame, RM32[Base]), RegIdx, RegScale;
			ULONG EffectiveAddr, Displacement = 0;

			if ((Base == 5) && (Mod == 0)) { // 32 bit unsigned displacement, no base register used
				Displacement = *reinterpret_cast<PULONG>(++CurrAddr);
				RegBase = 0;
			}
			else if (Mod == 1) { // 8 bit signed displacement
				PCHAR TempAddr = reinterpret_cast<PCHAR>(++CurrAddr);
				Displacement = static_cast<INT>(*TempAddr);
			}
			else { // 32 bit unsigned displacement
				Displacement = *reinterpret_cast<PULONG>(++CurrAddr);
			}

			if (Base == 4) {
				// The trap frame doesn't store the esp (because the cpu is always in supervisor mode on xbox), so we grab it from the function argument instead
				RegBase = HwEsp;
			}

			RegIdx = Index == 4 ? 0 : GETREG(TrapFrame, RM32[Index]);
			RegScale = 1 << Scale;
			EffectiveAddr = RegBase + RegIdx * RegScale + Displacement;
			if (EffectiveAddr) {
				Divisor = OperandSize == 8 ? *(PBYTE)EffectiveAddr : (OperandSize == 16 ? *(PUSHORT)EffectiveAddr : *(PULONG)EffectiveAddr);
			}
		}
		else {
			// No SIB byte present
			if (Mod == 3) {
				// Operand is a register
				ULONG ValueMask = OperandSize == 32 ? 0xFFFFFFFF : (OperandSize == 16 ? 0xFFFF : 0xFF);
				if ((OperandSize != 8) && (Rm == 4)) {
					// The trap frame doesn't store the esp (because the cpu is always in supervisor mode on xbox), so we grab it from the function argument instead
					Divisor = HwEsp & ValueMask;
				}
				else {
					Divisor = (GETREG(TrapFrame, OperandSize == 8 ? RM8[Rm] : RM32[Rm]) & ValueMask);
				}
			}
			else {
				// Operand is a memory location
				ULONG EffectiveAddr, Displacement = 0;
				if ((Mod == 0) && (Rm == 5)) { // 32 bit unsigned displacement, no register used
					EffectiveAddr = Displacement = *reinterpret_cast<PULONG>(++CurrAddr);
				}
				else {
					if (Mod == 1) { // 8 bit signed displacement
						PCHAR TempAddr = reinterpret_cast<PCHAR>(++CurrAddr);
						Displacement = static_cast<INT>(*TempAddr);
					}
					else if (Mod == 2) { // 32 bit unsigned displacement
						Displacement = *reinterpret_cast<PULONG>(++CurrAddr);
					}

					EffectiveAddr = GETREG(TrapFrame, RM32[Rm]) + Displacement;
				}

				if (EffectiveAddr) {
					Divisor = OperandSize == 8 ? *(PBYTE)EffectiveAddr : (OperandSize == 16 ? *(PUSHORT)EffectiveAddr : *(PULONG)EffectiveAddr);
				}
			}
		}

		if (Divisor) {
			Status = STATUS_INTEGER_OVERFLOW;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		// Just fallthrough
	}

	return Status;
}
