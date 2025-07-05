/*
* ergo720                Copyright (c) 2025
*/

#pragma once


static inline VOID CDECL outl(USHORT Port, ULONG Value)
{
	ASM_BEGIN
		ASM(mov eax, Value);
		ASM(mov dx, Port);
		ASM(out dx, eax);
	ASM_END
}

static inline VOID CDECL outw(USHORT Port, USHORT Value)
{
	ASM_BEGIN
		ASM(mov ax, Value);
		ASM(mov dx, Port);
		ASM(out dx, ax);
	ASM_END
}

static inline VOID CDECL outb(USHORT Port, BYTE Value)
{
	ASM_BEGIN
		ASM(mov al, Value);
		ASM(mov dx, Port);
		ASM(out dx, al);
	ASM_END
}

static inline ULONG CDECL inl(USHORT Port)
{
	ASM_BEGIN
		ASM(mov dx, Port);
		ASM(in eax, dx);
	ASM_END
}

static inline USHORT CDECL inw(USHORT Port)
{
	ASM_BEGIN
		ASM(mov dx, Port);
		ASM(in ax, dx);
	ASM_END
}

static inline BYTE CDECL inb(USHORT Port)
{
	ASM_BEGIN
		ASM(mov dx, Port);
		ASM(in al, dx);
	ASM_END
}

static inline VOID CDECL enable()
{
	ASM(sti);
}

static inline VOID CDECL disable()
{
	ASM(cli);
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
