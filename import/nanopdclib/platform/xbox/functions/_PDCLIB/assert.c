#include <assert.h>
#include <stdio.h>

// NOTE: cannot include rtl.hpp here, because that depends on types.hpp, which uses C++ specific "using" keyword
void __stdcall RtlAssert(void *FailedAssertion, void *FileName, uint32_t LineNumber, char *Message);

void _xbox_assert(char const *const expression, char const *const file_name, char const *const function_name, unsigned long line)
{
	char buffer[512];
	snprintf(buffer, 512, "In function '%s': ", function_name);
	RtlAssert((void *)expression, (void *)file_name, line, buffer);
}
