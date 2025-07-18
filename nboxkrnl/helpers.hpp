/*
* ergo720                Copyright (c) 2025
*/

#pragma once


static inline VOID outl(USHORT Port, ULONG Value)
{
	ASM_BEGIN
		ASM(mov eax, Value);
		ASM(mov dx, Port);
		ASM(out dx, eax);
	ASM_END
}

static inline VOID outw(USHORT Port, USHORT Value)
{
	ASM_BEGIN
		ASM(mov ax, Value);
		ASM(mov dx, Port);
		ASM(out dx, ax);
	ASM_END
}

static inline VOID outb(USHORT Port, BYTE Value)
{
	ASM_BEGIN
		ASM(mov al, Value);
		ASM(mov dx, Port);
		ASM(out dx, al);
	ASM_END
}

static inline ULONG inl(USHORT Port)
{
	ASM_BEGIN
		ASM(mov dx, Port);
		ASM(in eax, dx);
	ASM_END
}

static inline USHORT inw(USHORT Port)
{
	ASM_BEGIN
		ASM(mov dx, Port);
		ASM(in ax, dx);
	ASM_END
}

static inline BYTE inb(USHORT Port)
{
	ASM_BEGIN
		ASM(mov dx, Port);
		ASM(in al, dx);
	ASM_END
}

static inline VOID enable()
{
	ASM(sti);
}

static inline VOID disable()
{
	ASM(cli);
}

static inline DWORD save_int_state_and_disable()
{
	DWORD Eflags;

	ASM_BEGIN
		ASM(pushfd);
		ASM(cli);
		ASM(pop Eflags);
	ASM_END

	return Eflags;
}

static inline VOID restore_int_state(DWORD Eflags)
{
	ASM_BEGIN
		ASM(push Eflags);
		ASM(popfd);
	ASM_END
}

static inline UCHAR bit_scan_forward(ULONG *Index, ULONG Mask)
{
	if (Mask == 0) {
		return 0;
	}

	ULONG Value;
	ASM_BEGIN
		ASM(bsf eax, Mask);
		ASM(mov Value, eax);
	ASM_END
	*Index = Value;

	return 1;
}

static inline UCHAR bit_scan_reverse(ULONG *Index, ULONG Mask)
{
	if (Mask == 0) {
		return 0;
	}

	ULONG Value;
	ASM_BEGIN
		ASM(bsr eax, Mask);
		ASM(mov Value, eax);
	ASM_END
	*Index = Value;

	return 1;
}

static inline VOID CDECL atomic_store64(LONGLONG *dst, LONGLONG val)
{
	ASM_BEGIN
		ASM(pushfd);
		ASM(cli);
	ASM_END

	*dst = val;

	ASM(popfd);
}

static inline VOID CDECL atomic_add64(LONGLONG *dst, LONGLONG val)
{
	ASM_BEGIN
		ASM(pushfd);
		ASM(cli);
	ASM_END

	LONGLONG temp = *dst;
	temp += val;
	*dst = temp;

	ASM(popfd);
}
