/*
* ergo720                Copyright (c) 2025
* LukeUsher              Copyright (c) 2018
*/

#include "ex.hpp"
#include "hal.hpp"
#include "halp.hpp"
#include "rtl.hpp"
#include "av.hpp"

#define AV_PACK_NONE                      0x00000000
#define AV_PACK_STANDARD                  0x00000001
#define AV_PACK_RFU                       0x00000002
#define AV_PACK_SCART                     0x00000003
#define AV_PACK_HDTV                      0x00000004
#define AV_PACK_VGA                       0x00000005
#define AV_PACK_SVIDEO                    0x00000006
#define AV_PACK_MAX                       0x00000007
#define AV_PACK_MASK                      0x000000FF

#define AV_STANDARD_NTSC_M                0x00000100
#define AV_STANDARD_NTSC_J                0x00000200
#define AV_STANDARD_PAL_I                 0x00000300
#define AV_STANDARD_PAL_M                 0x00000400
#define AV_STANDARD_MAX                   0x00000500
#define AV_STANDARD_MASK                  0x00000F00
#define AV_STANDARD_SHIFT                 8

#define AV_FLAGS_HDTV_480i                0x00000000 // Bogus 'flag'
#define AV_FLAGS_HDTV_720p                0x00020000
#define AV_FLAGS_HDTV_1080i               0x00040000
#define AV_FLAGS_HDTV_480p                0x00080000
#define AV_HDTV_MODE_MASK                 0x000E0000 // Exclude AV_FLAGS_WIDESCREEN !

#define AV_FLAGS_NORMAL                   0x00000000
#define AV_FLAGS_WIDESCREEN               0x00010000
#define AV_FLAGS_LETTERBOX                0x00100000
#define AV_ASPECT_RATIO_MASK              0x00110000 // = AV_FLAGS_WIDESCREEN | AV_FLAGS_LETTERBOX

#define AV_FLAGS_INTERLACED               0x00200000

#define AV_FLAGS_60Hz                     0x00400000
#define AV_FLAGS_50Hz                     0x00800000
#define AV_REFRESH_MASK                   0x00C00000 // = AV_FLAGS_60Hz | AV_FLAGS_50Hz

#define AV_FLAGS_FIELD                    0x01000000
#define AV_FLAGS_10x11PAR                 0x02000000

#define AV_USER_FLAGS_MASK                (AV_HDTV_MODE_MASK | AV_ASPECT_RATIO_MASK | AV_REFRESH_MASK) // TODO : Should AV_REFRESH_MASK be AV_FLAGS_60Hz instead?
#define AV_USER_FLAGS_SHIFT               16

// Options to AvSendTVEncoderOption
#define AV_OPTION_MACROVISION_MODE        1
#define AV_OPTION_ENABLE_CC               2
#define AV_OPTION_DISABLE_CC              3
#define AV_OPTION_SEND_CC_DATA            4
#define AV_QUERY_CC_STATUS                5
#define AV_QUERY_AV_CAPABILITIES          6
#define AV_OPTION_BLANK_SCREEN            9
#define AV_OPTION_MACROVISION_COMMIT      10
#define AV_OPTION_FLICKER_FILTER          11
#define AV_OPTION_ZERO_MODE               12
#define AV_OPTION_QUERY_MODE              13
#define AV_OPTION_ENABLE_LUMA_FILTER      14
#define AV_OPTION_GUESS_FIELD             15
#define AV_QUERY_ENCODER_TYPE             16
#define AV_QUERY_MODE_TABLE_VERSION       17
#define AV_OPTION_CGMS                    18
#define AV_OPTION_WIDESCREEN              19


// Source: Cxbx-Reloaded
static ULONG AvpSMCVideoModeToAVPack(ULONG VideoMode)
{
	// NOTE: XboxType is set when the kernel initializes, and never changes afterwards

	switch (VideoMode)
	{
	case 0x0: return (XboxType == CONSOLE_CHIHIRO) ? AV_PACK_VGA : AV_PACK_SCART; // Scart on Retail/Debug, VGA on Chihiro
	case 0x1: return AV_PACK_HDTV;
	case 0x2: return AV_PACK_VGA;
	case 0x3: return AV_PACK_RFU;
	case 0x4: return AV_PACK_SVIDEO;
	case 0x6: return AV_PACK_STANDARD;
	}

	return AV_PACK_NONE;
}

// Source: Cxbx-Reloaded
static ULONG AvpQueryAvCapabilities()
{
	ULONG AvPack = AvpSMCVideoModeToAVPack(HalBootSMCVideoMode);
	ULONG Type, ResultSize;

	// First, read the factory AV settings
	ULONG AvRegion;
	NTSTATUS Status = ExQueryNonVolatileSetting(
		XC_FACTORY_AV_REGION,
		(DWORD *)&Type,
		&AvRegion,
		sizeof(ULONG),
		(PSIZE_T)&ResultSize);

	// If this failed, default to AV_STANDARD_NTSC_M | AV_FLAGS_60Hz
	if ((Status != STATUS_SUCCESS) || (ResultSize != sizeof(ULONG))) {
		AvRegion = AV_STANDARD_NTSC_M | AV_FLAGS_60Hz;
	}

	// Read the user-configurable (via the dashboard) settings
	ULONG UserSettings;
	Status = ExQueryNonVolatileSetting(
		XC_VIDEO,
		(DWORD *)&Type,
		&UserSettings,
		sizeof(ULONG),
		(PSIZE_T)&ResultSize);

	// If this failed, default to no user-options set
	if ((Status != STATUS_SUCCESS) || (ResultSize != sizeof(ULONG))) {
		UserSettings = 0;
	}

	return AvPack | (AvRegion & (AV_STANDARD_MASK | AV_REFRESH_MASK)) | (UserSettings & ~(AV_STANDARD_MASK | AV_PACK_MASK));
}

static VOID AvpSetFlickerFilter(ULONG Param)
{
	// This function sets the flicker filter used by the video encoder. We only support the conexant encoder here
	// "Param" specifies the filter level to use. 0=disable, [1-5]=valid levels
	// reg 0x34: ADPT_FF bit[7] -> disable(=0)/enable(=1) adaptive flicker filter
	// reg 0x34: C_ALTFF bit[3-4] -> chroma alternate flicker filter selection (only effective when ADPT_FF=1)
	// reg 0x34: Y_ALTFF bit[0-1] -> luma alternate flicker filter selection (only effective when ADPT_FF=1)
	// reg 0xC8: DIS_YLPF bit[7] -> disable(=1)/enable(=0) luma initial horizontal low pass filter
	// reg 0xC8: DIS_FFILT bit[6] -> disable(=1)/enable(=0) standard flicker filter
	// reg 0xC8: F_SELC bit[3-5] -> chroma standard flicker filter
	// reg 0xC8: F_SELY bit[0-2] -> luma standard flicker filter

	if (Param == 0) {
		ULONG Value = 0;
		HalReadSMBusValue(CONEXANT_READ_ADDR, 0xC8, FALSE, &Value);
		Value |= 0x40; // disable standard filter
		HalWriteSMBusValue(CONEXANT_WRITE_ADDR, 0xC8, FALSE, Value);
	}
	else {
		ULONG Level, Value = 0;
		if (Param == 1) {
			Level = 1; // selects lv 2
		}
		else if (Param == 2) {
			Level = 2; // selects lv 3
		}
		else if (Param == 3) {
			Level = 3; // selects lv 4
		}
		else {
			Level = 0; // selects lv 5 (max)
		}

		HalReadSMBusValue(CONEXANT_READ_ADDR, 0xC8, FALSE, &Value);
		Value &= 0x80; // preserve DIS_YLPF and clear the rest
		Value |= (Level | (Level << 3)); // set level for chroma and luma
		HalWriteSMBusValue(CONEXANT_WRITE_ADDR, 0xC8, FALSE, Value);
		HalWriteSMBusValue(CONEXANT_WRITE_ADDR, 0x34, FALSE, Param == 5 ? 0x80 : 0x00); // also enable the adaptive filter if we have set the standard filter at max level
	}
}

static VOID AvpSetLumaFilter(ULONG Param)
{
	// This function sets the luma post-flicker filter/scaler horizontal low pass filter used by the video encoder. We only support the conexant encoder here
	// "Param" specifies the luma filter level to use. 0=disable, 1=enable
	// reg 0x96: CLPF bit[6-7] -> bypass(=0) chroma post-flicker filter/scaler horizontal low pass filter
	// reg 0x96: YLPF bit[4-5] -> bypass(=0)/LPF1 setting(=1) luma post-flicker filter/scaler horizontal low pass filter

	ULONG Value = 0;
	HalReadSMBusValue(CONEXANT_READ_ADDR, 0x96, FALSE, &Value);
	Value &= 0x0F; // bypass chroma filter
	if (Param) {
		Value |= 0x10; // enable LPF1 setting
	}
	HalWriteSMBusValue(CONEXANT_WRITE_ADDR, 0x96, FALSE, Value);
}

EXPORTNUM(2) VOID XBOXAPI AvSendTVEncoderOption
(
	PVOID RegisterBase,
	ULONG Option,
	ULONG Param,
	ULONG *Result
)
{
	switch (Option)
	{
	case AV_QUERY_AV_CAPABILITIES:
		*Result = AvpQueryAvCapabilities();
		break;

	case AV_OPTION_FLICKER_FILTER:
		AvpSetFlickerFilter(Param);
		break;

	case AV_OPTION_ENABLE_LUMA_FILTER:
		AvpSetLumaFilter(Param);
		break;

	default:
		RIP_API_FMT("Option 0x%08X with Param 0x%08X not implemented!", Option, Param);
	}
}
