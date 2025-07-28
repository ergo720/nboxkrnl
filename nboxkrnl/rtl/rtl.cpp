/*
 * ergo720                Copyright (c) 2023
 * Fisherman166           Copyright (c) 2018
 * PatrickvL              Copyright (c) 2018
 */

#include "rtl.hpp"
#include "..\hal\halp.hpp"
#include "..\dbg\dbg.hpp"
#include "..\ex\ex.hpp"
#include "..\mm\mm.hpp"
#include <string.h>
#include <assert.h>
#include <nanoprintf.h>

#define TICKSPERSEC        10000000
#define TICKSPERMSEC       10000
#define MSECSPERSEC        1000
#define SECSPERDAY         86400
#define SECSPERHOUR        3600
#define SECSPERMIN         60
#define MINSPERHOUR        60
#define HOURSPERDAY        24
#define EPOCHWEEKDAY       1  /* Jan 1, 1601 was Monday */
#define DAYSPERWEEK        7
#define MONSPERYEAR        12
#define DAYSPERQUADRICENTENNIUM (365 * 400 + 97)
#define DAYSPERNORMALQUADRENNIUM (365 * 4 + 1)

 /* 1601 to 1970 is 369 years plus 89 leap days */
#define SECS_1601_TO_1970  ((369 * 365 + 89) * (ULONGLONG)SECSPERDAY)
#define TICKS_1601_TO_1970 (SECS_1601_TO_1970 * TICKSPERSEC)
/* 1601 to 1980 is 379 years plus 91 leap days */
#define SECS_1601_TO_1980  ((379 * 365 + 91) * (ULONGLONG)SECSPERDAY)
#define TICKS_1601_TO_1980 (SECS_1601_TO_1980 * TICKSPERSEC)


EXPORTNUM(264) VOID XBOXAPI RtlAssert
(
	PVOID FailedAssertion,
	PVOID FileName,
	ULONG LineNumber,
	PCHAR Message
)
{
	// This routine is called from pdclib when an assertion fails in debug builds only

	DbgPrint("\n\n!!! Assertion failed: %s%s !!!\nSource File: %s, line %ld\n\n", Message ? Message : "", FailedAssertion, FileName, LineNumber);

	// NOTE: this should check for the presence of a guest debugger on the host side (such as lib86dbg) and, if present, communicate that it should hook
	// the breakpoint exception handler that int3 generates, so that it can take over us and start a debugging session

	ASM(int 3);
}

EXPORTNUM(265) __declspec(naked) VOID XBOXAPI RtlCaptureContext
(
	PCONTEXT ContextRecord
)
{
	// NOTE: this function sets esp and ebp to the values they had in the caller's caller. For example, when called from RtlUnwind, it will set them to
	// the values used in _global_unwind2. To achieve this effect, the caller must use __declspec(noinline) and #pragma optimize("y", off)
	ASM_BEGIN
		ASM(push ebx);
		ASM(mov ebx, [esp + 8]);        // ebx = ContextRecord;

		ASM(mov [ebx]CONTEXT.Eax, eax); // ContextRecord->Eax = eax;
		ASM(mov eax, [esp]);            // eax = original value of ebx
		ASM(mov [ebx]CONTEXT.Ebx, eax); // ContextRecord->Ebx = original value of ebx
		ASM(mov [ebx]CONTEXT.Ecx, ecx); // ContextRecord->Ecx = ecx;
		ASM(mov [ebx]CONTEXT.Edx, edx); // ContextRecord->Edx = edx;
		ASM(mov [ebx]CONTEXT.Esi, esi); // ContextRecord->Esi = esi;
		ASM(mov [ebx]CONTEXT.Edi, edi); // ContextRecord->Edi = edi;

		ASM(mov word ptr [ebx]CONTEXT.SegCs, cs); // ContextRecord->SegCs = cs;
		ASM(mov word ptr [ebx]CONTEXT.SegSs, ss); // ContextRecord->SegSs = ss;
		ASM(pushfd);
		ASM(pop [ebx]CONTEXT.EFlags);   // ContextRecord->EFlags = flags;

		ASM(mov eax, [ebp]);            // eax = old ebp;
		ASM(mov [ebx]CONTEXT.Ebp, eax); // ContextRecord->Ebp = ebp;
		ASM(mov eax, [ebp + 4]);        // eax = return address;
		ASM(mov [ebx]CONTEXT.Eip, eax); // ContextRecord->Eip = return address;
		ASM(lea eax, [ebp + 8]);
		ASM(mov [ebx]CONTEXT.Esp, eax); // ContextRecord->Esp = original esp value;

		ASM(pop ebx);
		ASM(ret 4);
	ASM_END
}

// Source: Cxbx-Reloaded
EXPORTNUM(266) USHORT XBOXAPI RtlCaptureStackBackTrace
(
	ULONG FramesToSkip,
	ULONG FramesToCapture,
	PVOID *BackTrace,
	PULONG BackTraceHash
)
{
	PVOID Frames[2 * 64];
	ULONG FrameCount;
	ULONG Hash = 0;
	USHORT i;

	/* Skip a frame for the caller */
	FramesToSkip++;

	/* Don't go past the limit */
	if ((FramesToCapture + FramesToSkip) >= 128) {
		return 0;
	}

	/* Do the back trace */
	FrameCount = RtlWalkFrameChain(Frames, FramesToCapture + FramesToSkip, 0);

	/* Make sure we're not skipping all of them */
	if (FrameCount <= FramesToSkip) {
		return 0;
	}

	/* Loop all the frames */
	for (i = 0; i < FramesToCapture; i++) {
		/* Don't go past the limit */
		if ((FramesToSkip + i) >= FrameCount) {
			break;
		}

		/* Save this entry and hash it */
		BackTrace[i] = Frames[FramesToSkip + i];
		Hash += reinterpret_cast<ULONG>(BackTrace[i]);
	}

	/* Write the hash */
	if (BackTraceHash) {
		*BackTraceHash = Hash;
	}

	/* Clear the other entries and return count */
	RtlFillMemoryUlong(Frames, 128, 0);

	return i;
}

EXPORTNUM(277) VOID XBOXAPI RtlEnterCriticalSection
(
	PRTL_CRITICAL_SECTION CriticalSection
)
{
	// This function must update the members of CriticalSection atomically, so we use assembly

	ASM_BEGIN
		ASM(mov ecx, CriticalSection);
		ASM(mov eax, [KiPcr]KPCR.PrcbData.CurrentThread);
		ASM(inc [ecx]RTL_CRITICAL_SECTION.LockCount);
		ASM(jnz already_owned);
		ASM(mov [ecx]RTL_CRITICAL_SECTION.OwningThread, eax);
		ASM(mov [ecx]RTL_CRITICAL_SECTION.RecursionCount, 1);
		ASM(jmp end_func);
	already_owned:
		ASM(cmp [ecx]RTL_CRITICAL_SECTION.OwningThread, eax);
		ASM(jz owned_by_self);
		ASM(push eax);
		ASM(push 0);
		ASM(push FALSE);
		ASM(push KernelMode);
		ASM(push WrExecutive);
		ASM(push ecx);
		ASM(call KeWaitForSingleObject);
		ASM(pop [ecx]RTL_CRITICAL_SECTION.OwningThread);
		ASM(mov [ecx]RTL_CRITICAL_SECTION.RecursionCount, 1);
		ASM(jmp end_func);
	owned_by_self:
		ASM(inc [ecx]RTL_CRITICAL_SECTION.RecursionCount);
	end_func:
	ASM_END
}

EXPORTNUM(278) VOID XBOXAPI RtlEnterCriticalSectionAndRegion
(
	PRTL_CRITICAL_SECTION CriticalSection
)
{
	KeEnterCriticalRegion();
	RtlEnterCriticalSection(CriticalSection);
}

// Source: Cxbx-Reloaded
EXPORTNUM(281) LARGE_INTEGER XBOXAPI RtlExtendedIntegerMultiply
(
	LARGE_INTEGER Multiplicand,
	LONG Multiplier
)
{
	LARGE_INTEGER Product;
	Product.QuadPart = Multiplicand.QuadPart * (LONGLONG)Multiplier;

	return Product;
}

// Source: Cxbx-Reloaded
EXPORTNUM(282) LARGE_INTEGER XBOXAPI RtlExtendedLargeIntegerDivide
(
	LARGE_INTEGER Dividend,
	ULONG Divisor,
	PULONG Remainder
)
{
	LARGE_INTEGER quotient = Dividend;

	if (Divisor == 0) {
		RtlRaiseStatus(STATUS_INTEGER_DIVIDE_BY_ZERO);
	}
	else {
		ULONG local_remainder = 0;
		BOOLEAN carry, remainder_carry;

		// Binary division algorithm reverse engineered from real hardware.
		for (uint8_t i = 64; i > 0; i--) {
			carry = (quotient.QuadPart >> 63) & 0x1;
			remainder_carry = (local_remainder >> 31) & 0x1;
			quotient.QuadPart <<= 1;
			local_remainder = (local_remainder << 1) | carry;
			if (remainder_carry || (local_remainder >= Divisor)) {
				quotient.u.LowPart += 1;
				local_remainder -= Divisor;
			}
		}
		if (Remainder) {
			*Remainder = local_remainder;
		}
	}

	return quotient;
}

// Source: Cxbx-Reloaded
EXPORTNUM(283) LARGE_INTEGER XBOXAPI RtlExtendedMagicDivide
(
	LARGE_INTEGER Dividend,
	LARGE_INTEGER MagicDivisor,
	CCHAR ShiftCount
)
{
	LARGE_INTEGER Result;

	ULONGLONG DividendHigh;
	ULONGLONG DividendLow;
	ULONGLONG InverseDivisorHigh;
	ULONGLONG InverseDivisorLow;
	ULONGLONG AhBl;
	ULONGLONG AlBh;
	BOOLEAN Positive;

	if (Dividend.QuadPart < 0) {
		DividendHigh = UPPER_32((ULONGLONG)-Dividend.QuadPart);
		DividendLow = LOWER_32((ULONGLONG)-Dividend.QuadPart);
		Positive = FALSE;
	}
	else {
		DividendHigh = UPPER_32((ULONGLONG)Dividend.QuadPart);
		DividendLow = LOWER_32((ULONGLONG)Dividend.QuadPart);
		Positive = TRUE;
	}

	InverseDivisorHigh = UPPER_32((ULONGLONG)MagicDivisor.QuadPart);
	InverseDivisorLow = LOWER_32((ULONGLONG)MagicDivisor.QuadPart);

	AhBl = DividendHigh * InverseDivisorLow;
	AlBh = DividendLow * InverseDivisorHigh;

	Result.QuadPart =
		(LONGLONG)((DividendHigh * InverseDivisorHigh +
			UPPER_32(AhBl) +
			UPPER_32(AlBh) +
			UPPER_32(LOWER_32(AhBl) + LOWER_32(AlBh) +
				UPPER_32(DividendLow * InverseDivisorLow))) >> ShiftCount);
	if (!Positive) {
		Result.QuadPart = -Result.QuadPart;
	}

	return Result;
}

// Source: Cxbx-Reloaded
EXPORTNUM(284) VOID XBOXAPI RtlFillMemory
(
	VOID *Destination,
	DWORD Length,
	BYTE  Fill
)
{
	memset(Destination, Fill, Length);
}

EXPORTNUM(285) VOID XBOXAPI RtlFillMemoryUlong
(
	PVOID Destination,
	SIZE_T Length,
	ULONG Pattern
)
{
	assert(Length);
	assert((Length % sizeof(ULONG)) == 0); // Length must be a multiple of ULONG
	assert(((ULONG_PTR)Destination % sizeof(ULONG)) == 0); // Destination must be 4-byte aligned

	unsigned NumOfRepeats = Length / sizeof(ULONG);
	PULONG d = (PULONG)Destination;

	for (unsigned i = 0; i < NumOfRepeats; ++i) {
		d[i] = Pattern; // copy an ULONG at a time
	}
}

// Source: Cxbx-Reloaded
EXPORTNUM(287) VOID XBOXAPI RtlFreeUnicodeString
(
	PUNICODE_STRING UnicodeString
)
{
	if (UnicodeString->Buffer) {
		ExFreePool(UnicodeString->Buffer);
		memset(UnicodeString, 0, sizeof(*UnicodeString));
	}
}

EXPORTNUM(291) VOID XBOXAPI RtlInitializeCriticalSection
(
	PRTL_CRITICAL_SECTION CriticalSection
)
{
	KeInitializeEvent((PKEVENT)&CriticalSection->Event, SynchronizationEvent, FALSE);
	CriticalSection->LockCount = -1;
	CriticalSection->RecursionCount = 0;
	CriticalSection->OwningThread = 0;
}

EXPORTNUM(294) VOID XBOXAPI RtlLeaveCriticalSection
(
	PRTL_CRITICAL_SECTION CriticalSection
)
{
	// This function must update the members of CriticalSection atomically, so we use assembly

	ASM_BEGIN
		ASM(mov ecx, CriticalSection);
		ASM(dec [ecx]RTL_CRITICAL_SECTION.RecursionCount);
		ASM(jnz dec_count);
		ASM(dec [ecx]RTL_CRITICAL_SECTION.LockCount);
		ASM(mov [ecx]RTL_CRITICAL_SECTION.OwningThread, 0);
		ASM(jl end_func);
		ASM(push FALSE);
		ASM(push PRIORITY_BOOST_EVENT);
		ASM(push ecx);
		ASM(call KeSetEvent);
		ASM(jmp end_func);
	dec_count:
		ASM(dec [ecx]RTL_CRITICAL_SECTION.LockCount);
	end_func:
	ASM_END
}

EXPORTNUM(295) VOID XBOXAPI RtlLeaveCriticalSectionAndRegion
(
	PRTL_CRITICAL_SECTION CriticalSection
)
{
	// NOTE: this must check RecursionCount only once, so that it can unconditionally call KeLeaveCriticalRegion if the counter is zero regardless of
	// its current value. This, to guard against the case where a thread switch happens after the counter is updated but before KeLeaveCriticalRegion is called

	ASM_BEGIN
		ASM(mov ecx, CriticalSection);
		ASM(dec dword ptr [ecx]RTL_CRITICAL_SECTION.RecursionCount);
		ASM(jnz dec_count);
		ASM(dec dword ptr [ecx]RTL_CRITICAL_SECTION.LockCount);
		ASM(mov dword ptr [ecx]RTL_CRITICAL_SECTION.OwningThread, 0);
		ASM(jl not_signalled);
		ASM(push FALSE);
		ASM(push PRIORITY_BOOST_EVENT);
		ASM(push ecx);
		ASM(call KeSetEvent);
	not_signalled:
		ASM(call KeLeaveCriticalRegion);
		ASM(jmp end_func);
	dec_count:
		ASM(dec dword ptr [ecx]RTL_CRITICAL_SECTION.LockCount);
	end_func:
	ASM_END
}

// Source: Cxbx-Reloaded
EXPORTNUM(297) VOID XBOXAPI RtlMapGenericMask
(
	PACCESS_MASK AccessMask,
	PGENERIC_MAPPING GenericMapping
)
{
	if (*AccessMask & GENERIC_READ) {
		*AccessMask |= GenericMapping->GenericRead;
	}
	if (*AccessMask & GENERIC_WRITE) {
		*AccessMask |= GenericMapping->GenericWrite;
	}
	if (*AccessMask & GENERIC_EXECUTE) {
		*AccessMask |= GenericMapping->GenericExecute;
	}
	if (*AccessMask & GENERIC_ALL) {
		*AccessMask |= GenericMapping->GenericAll;
	}

	*AccessMask &= ~(GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE | GENERIC_ALL);
}

// Source: Cxbx-Reloaded
EXPORTNUM(298) VOID XBOXAPI RtlMoveMemory
(
	VOID *Destination,
	VOID *Source,
	SIZE_T Length
)
{
	memmove(Destination, Source, Length);
}

static const int RtlpMonthLengths[2][MONSPERYEAR] =
{
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

static inline BOOL RtlpIsLeapYear(int Year)
{
	return Year % 4 == 0 && (Year % 100 != 0 || Year % 400 == 0);
}

// Source: Cxbx-Reloaded
EXPORTNUM(304) BOOLEAN XBOXAPI RtlTimeFieldsToTime
(
	PTIME_FIELDS TimeFields,
	PLARGE_INTEGER Time
)
{
	LONG Month, Year, Cleaps, Day;

	/* Verify each TimeFields' variables are within range */
	if (TimeFields->Millisecond < 0 || TimeFields->Millisecond > 999) {
		return FALSE;
	}
	if (TimeFields->Second < 0 || TimeFields->Second > 59) {
		return FALSE;
	}
	if (TimeFields->Minute < 0 || TimeFields->Minute > 59) {
		return FALSE;
	}
	if (TimeFields->Hour < 0 || TimeFields->Hour > 23) {
		return FALSE;
	}
	if (TimeFields->Month < 1 || TimeFields->Month > 12) {
		return FALSE;
	}
	if (TimeFields->Day < 1 ||
		TimeFields->Day > RtlpMonthLengths[RtlpIsLeapYear(TimeFields->Year)][TimeFields->Month - 1]) {
		return FALSE;
	}
	if (TimeFields->Year < 1601) {
		return FALSE;
	}

	/* now calculate a day count from the date
	* First start counting years from March. This way the leap days
	* are added at the end of the year, not somewhere in the middle.
	* Formula's become so much less complicate that way.
	* To convert: add 12 to the month numbers of Jan and Feb, and
	* take 1 from the year */
	if (TimeFields->Month < 3) {
		Month = TimeFields->Month + 13;
		Year = TimeFields->Year - 1;
	}
	else {
		Month = TimeFields->Month + 1;
		Year = TimeFields->Year;
	}
	Cleaps = (3 * (Year / 100) + 3) / 4;   /* nr of "century leap years"*/
	Day = (36525 * Year) / 100 - Cleaps +  /* year * DayPerYear, corrected */
		(1959 * Month) / 64 +              /* months * DayPerMonth */
		TimeFields->Day -                  /* day of the month */
		584817;                            /* zero that on 1601-01-01 */
	/* done */

	/* Convert into Time format */
	Time->QuadPart = Day * HOURSPERDAY;
	Time->QuadPart = (Time->QuadPart + TimeFields->Hour) * MINSPERHOUR;
	Time->QuadPart = (Time->QuadPart + TimeFields->Minute) * SECSPERMIN;
	Time->QuadPart = (Time->QuadPart + TimeFields->Second) * MSECSPERSEC;
	Time->QuadPart = (Time->QuadPart + TimeFields->Millisecond) * TICKSPERMSEC;

	return TRUE;
}

constexpr LARGE_INTEGER Magic10000 = { .QuadPart = 0xd1b71758e219652ci64 };
#define SHIFT10000 13
constexpr LARGE_INTEGER Magic86400000 = { .QuadPart = 0xc6d750ebfa67b90ei64 };
#define SHIFT86400000 26

// Source: Cxbx-Reloaded
EXPORTNUM(305) VOID XBOXAPI RtlTimeToTimeFields
(
	PLARGE_INTEGER Time,
	PTIME_FIELDS TimeFields
)
{
	LONGLONG Days, Cleaps, Years, Yearday, Months;

	/* Extract milliseconds from time and days from milliseconds */
	// NOTE: Reverse engineered native kernel uses RtlExtendedMagicDivide calls.
	// Using similar code of ReactOS does not emulate native kernel's implement for
	// one increment over large integer's max value.
	LARGE_INTEGER MillisecondsTotal = RtlExtendedMagicDivide(*Time, Magic10000, SHIFT10000);
	Days = RtlExtendedMagicDivide(MillisecondsTotal, Magic86400000, SHIFT86400000).u.LowPart;
	MillisecondsTotal.QuadPart -= Days * SECSPERDAY * MSECSPERSEC;

	/* compute time of day */
	TimeFields->Millisecond = MillisecondsTotal.u.LowPart % MSECSPERSEC;
	DWORD RemainderTime = MillisecondsTotal.u.LowPart / MSECSPERSEC;
	TimeFields->Second = RemainderTime % SECSPERMIN;
	RemainderTime /= SECSPERMIN;
	TimeFields->Minute = RemainderTime % MINSPERHOUR;
	RemainderTime /= MINSPERHOUR;
	TimeFields->Hour = RemainderTime; // NOTE: Remaining hours did not received 24 hours modulo treatment.

	/* compute day of week */
	TimeFields->Weekday = (EPOCHWEEKDAY + Days) % DAYSPERWEEK;

	/* compute year, month and day of month. */
	Cleaps = (3 * ((4 * Days + 1227) / DAYSPERQUADRICENTENNIUM) + 3) / 4;
	Days += 28188 + Cleaps;
	Years = (20 * Days - 2442) / (5 * DAYSPERNORMALQUADRENNIUM);
	Yearday = Days - (Years * DAYSPERNORMALQUADRENNIUM) / 4;
	Months = (64 * Yearday) / 1959;
	/* the result is based on a year starting on March.
	* To convert take 12 from January and February and
	* increase the year by one. */
	if (Months < 14) {
		TimeFields->Month = (USHORT)(Months - 1);
		TimeFields->Year = (USHORT)(Years + 1524);
	}
	else {
		TimeFields->Month = (USHORT)(Months - 13);
		TimeFields->Year = (USHORT)(Years + 1525);
	}
	/* calculation of day of month is based on the wonderful
	* sequence of INT( n * 30.6): it reproduces the
	* 31-30-31-30-31-31 month lengths exactly for small n's */
	TimeFields->Day = (USHORT)(Yearday - (1959 * Months) / 64);
}

// Source: Cxbx-Reloaded
EXPORTNUM(307) ULONG FASTCALL RtlUlongByteSwap
(
	ULONG Source
)
{
	return (Source >> 24) | ((Source & 0xFF0000) >> 8) | ((Source & 0xFF00) << 8) | ((Source & 0xFF) << 24);
}

// Source: Cxbx-Reloaded
EXPORTNUM(318) USHORT FASTCALL RtlUshortByteSwap
(
	USHORT Source
)
{
	return (Source >> 8) | ((Source & 0xFF) << 8);
}

// Source: Cxbx-Reloaded
EXPORTNUM(319) ULONG XBOXAPI RtlWalkFrameChain
(
	PVOID *Callers,
	ULONG Count,
	ULONG Flags
)
{
	// Note that this works even if there is a KTRAP_FRAME in the stack. This, because the exception handler sets ebp to the frame itself,
	// and its first two members are DbgEbp and DbgEip, the old values of ebp and eip of the caller

	ULONG_PTR Stack;
	/* Get current EBP */
	ASM(mov Stack, ebp);

	/* Get the actual stack limits */
	PKTHREAD Thread = KeGetCurrentThread();

	/* Start with defaults */
	ULONG_PTR StackBegin = reinterpret_cast<ULONG_PTR>(Thread->StackLimit); // base stack address
	ULONG_PTR StackEnd = reinterpret_cast<ULONG_PTR>(Thread->StackBase); // highest stack address

	/* Check if EBP is inside the stack */
	if ((StackBegin <= Stack) && (Stack <= StackEnd)) {
		/* Then make the stack start at EBP */
		StackBegin = Stack;
	}
	else {
		/* We're somewhere else entirely... use EBP for safety */
		StackBegin = Stack;
		StackEnd = ROUND_DOWN_4K(StackBegin);
	}

	ULONG i = 0;

	/* Use a SEH block for maximum protection */
	__try {
		/* Loop the frames */
		BOOLEAN StopSearch = FALSE;
		for (i = 0; i < Count; i++) {
			/*
			 * Leave if we're past the stack,
			 * if we're before the stack,
			 * or if we've reached ourselves.
			 */
			if ((Stack >= StackEnd) ||
				(!i ? (Stack < StackBegin) : (Stack <= StackBegin)) ||
				((StackEnd - Stack) < (2 * sizeof(ULONG_PTR))))
			{
				/* We're done or hit a bad address */
				break;
			}

			/* Get new stack and EIP */
			ULONG_PTR NewStack = *(ULONG_PTR *)Stack;
			ULONG Eip = *(ULONG_PTR *)(Stack + sizeof(ULONG_PTR));

			/* Check if Eip is not in the forbidden 64 KiB block */
			if (Eip < KiB(64)) {
				break;
			}

			/* Check if the new pointer is above the old one and past the end */
			if (!((Stack < NewStack) && (NewStack < StackEnd))) {
				/* Stop searching after this entry */
				StopSearch = TRUE;
			}

			/* Also make sure that the EIP isn't a stack address */
			if ((StackBegin < Eip) && (Eip < StackEnd)) {
				break;
			}

			/* Save this frame */
			Callers[i] = reinterpret_cast<PVOID>(Eip);

			/* Check if we should continue */
			if (StopSearch)
			{
				/* Return the next index */
				i++;
				break;
			}

			/* Move to the next stack */
			Stack = NewStack;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		/* No index */
		i = 0;
	}

	return i;
}

EXPORTNUM(320) VOID XBOXAPI RtlZeroMemory
(
	PVOID Destination,
	SIZE_T Length
)
{
	memset(Destination, 0, Length);
}

EXPORTNUM(352) VOID XBOXAPI RtlRip
(
	PVOID ApiName,
	PVOID Expression,
	PVOID Message
)
{
	// This routine terminates the system. It should be used when a failure is expected, for example when executing unimplemented stuff

	const char *OpenBracket = " (";
	const char *CloseBracket = ") ";

	if (!Message) {
		if (Expression) {
			Message = const_cast<PCHAR>("failed");
		}
		else {
			Message = const_cast<PCHAR>("execution failure");
		}
	}

	if (!Expression) {
		Expression = const_cast<PCHAR>("");
		OpenBracket = "";
		CloseBracket = " ";
	}

	if (!ApiName) {
		ApiName = const_cast<PCHAR>("RIP");
	}

	DbgPrint("%s:%s%s%s%s", ApiName, OpenBracket, Expression, CloseBracket, Message);

	// Capture a stack trace to help debugging this issue
	PVOID BackTrace[128] = { 0 };
	ULONG TraceHash;
	ULONG NumOfFrames = RtlCaptureStackBackTrace(0, 126, BackTrace, &TraceHash);
	DbgPrint("The kernel terminated execution with RtlRip. A stack trace was created with %u frames captured (Hash = 0x%X)", NumOfFrames, TraceHash);
	for (unsigned i = 0; (i < 128) && BackTrace[i]; ++i) {
		DbgPrint("Traced Eip at level %u is 0x%X", i, BackTrace[i]);
	}

	HalpShutdownSystem();
}

VOID CDECL RipWithMsg(const char *Func, const char *Msg, ...)
{
	char buff[512];
	va_list vlist;
	va_start(vlist, Msg);
	npf_vsnprintf(buff, sizeof(buff), Msg, vlist);
	va_end(vlist);

	RtlRip(const_cast<PCHAR>(Func), nullptr, const_cast<PCHAR>(buff));
}
