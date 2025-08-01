/*
 * ergo720                Copyright (c) 2023
 */

#include "av.hpp"
#include "dbg.hpp"
#include "ex.hpp"
#include "fsc.hpp"
#include "hal.hpp"
#include "ke.hpp"
#include "mm.hpp"
#include "nt.hpp"
#include "ob.hpp"
#include "ps.hpp"
#include "rtl.hpp"
#include "xc.hpp"
#include "xe.hpp"

#define FUNC(f) (f)
#define VARIABLE(v) (v)

ULONG KernelThunkTable[379] =
{
	(ULONG)FUNC(nullptr),                                  // 0x0000 (0) "Undefined", this function doesn't exist
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&AvGetSavedDataAddress),                   // 0x0001 (1)
	(ULONG)FUNC(&AvSendTVEncoderOption),                   // 0x0002 (2)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&AvSetDisplayMode),                        // 0x0003 (3)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&AvSetSavedDataAddress),                   // 0x0004 (4)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&DbgBreakPoint),                           // 0x0005 (5)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&DbgBreakPointWithStatus),                 // 0x0006 (6)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&DbgLoadImageSymbols),                     // 0x0007 (7) DEVKIT
	(ULONG)FUNC(&DbgPrint),                                // 0x0008 (8)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&HalReadSMCTrayState),                     // 0x0009 (9)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&DbgPrompt),                               // 0x000A (10)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&DbgUnLoadImageSymbols),                   // 0x000B (11) DEVKIT
	(ULONG)FUNC(&ExAcquireReadWriteLockExclusive),         // 0x000C (12)
	(ULONG)FUNC(&ExAcquireReadWriteLockShared),            // 0x000D (13)
	(ULONG)FUNC(&ExAllocatePool),                          // 0x000E (14)
	(ULONG)FUNC(&ExAllocatePoolWithTag),                   // 0x000F (15)
	(ULONG)VARIABLE(&ExEventObjectType),                   // 0x0010 (16)
	(ULONG)FUNC(&ExFreePool),                              // 0x0011 (17)
	(ULONG)FUNC(&ExInitializeReadWriteLock),               // 0x0012 (18)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&ExInterlockedAddLargeInteger),            // 0x0013 (19)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&ExInterlockedAddLargeStatistic),          // 0x0014 (20)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&ExInterlockedCompareExchange64),          // 0x0015 (21)
	(ULONG)VARIABLE(&ExMutantObjectType),                  // 0x0016 (22)
	(ULONG)FUNC(&ExQueryPoolBlockSize),                    // 0x0017 (23)
	(ULONG)FUNC(&ExQueryNonVolatileSetting),               // 0x0018 (24)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&ExReadWriteRefurbInfo),                   // 0x0019 (25)
	(ULONG)FUNC(&ExRaiseException),                        // 0x001A (26)
	(ULONG)FUNC(&ExRaiseStatus),                           // 0x001B (27)
	(ULONG)FUNC(&ExReleaseReadWriteLock),                  // 0x001C (28)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&ExSaveNonVolatileSetting),                // 0x001D (29)
	(ULONG)FUNC(nullptr), //(ULONG)VARIABLE(&ExSemaphoreObjectType),               // 0x001E (30)
	(ULONG)FUNC(nullptr), //(ULONG)VARIABLE(&ExTimerObjectType),                   // 0x001F (31)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&ExfInterlockedInsertHeadList),            // 0x0020 (32)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&ExfInterlockedInsertTailList),            // 0x0021 (33)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&ExfInterlockedRemoveHeadList),            // 0x0022 (34)
	(ULONG)FUNC(&FscGetCacheSize),                         // 0x0023 (35)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&FscInvalidateIdleBlocks),                 // 0x0024 (36)
	(ULONG)FUNC(&FscSetCacheSize),                         // 0x0025 (37)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&HalClearSoftwareInterrupt),               // 0x0026 (38)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&HalDisableSystemInterrupt),               // 0x0027 (39)
	(ULONG)VARIABLE(&HalDiskCachePartitionCount),          // 0x0028 (40)  A.k.a. "IdexDiskPartitionPrefixBuffer"
	(ULONG)FUNC(nullptr), //(ULONG)VARIABLE(&HalDiskModelNumber),                  // 0x0029 (41)
	(ULONG)FUNC(nullptr), //(ULONG)VARIABLE(&HalDiskSerialNumber),                 // 0x002A (42)
	(ULONG)FUNC(&HalEnableSystemInterrupt),                // 0x002B (43)
	(ULONG)FUNC(&HalGetInterruptVector),                   // 0x002C (44)
	(ULONG)FUNC(&HalReadSMBusValue),                       // 0x002D (45)
	(ULONG)FUNC(&HalReadWritePCISpace),                    // 0x002E (46)
	(ULONG)FUNC(&HalRegisterShutdownNotification),         // 0x002F (47)
	(ULONG)FUNC(&HalRequestSoftwareInterrupt),             // 0x0030 (48)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&HalReturnToFirmware),                     // 0x0031 (49)
	(ULONG)FUNC(&HalWriteSMBusValue),                      // 0x0032 (50)
	(ULONG)FUNC(&InterlockedCompareExchange),        // 0x0033 (51)
	(ULONG)FUNC(&InterlockedDecrement),              // 0x0034 (52)
	(ULONG)FUNC(&InterlockedIncrement),              // 0x0035 (53)
	(ULONG)FUNC(&InterlockedExchange),               // 0x0036 (54)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&InterlockedExchangeAdd),            // 0x0037 (55)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&InterlockedFlushSList),             // 0x0038 (56)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&InterlockedPopEntrySList),          // 0x0039 (57)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&InterlockedPushEntrySList),         // 0x003A (58)
	(ULONG)FUNC(&IoAllocateIrp),                           // 0x003B (59)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&IoBuildAsynchronousFsdRequest),           // 0x003C (60)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&IoBuildDeviceIoControlRequest),           // 0x003D (61)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&IoBuildSynchronousFsdRequest),            // 0x003E (62)
	(ULONG)FUNC(&IoCheckShareAccess),                      // 0x003F (63)
	(ULONG)FUNC(nullptr), //(ULONG)VARIABLE(&IoCompletionObjectType),              // 0x0040 (64)
	(ULONG)FUNC(&IoCreateDevice),                          // 0x0041 (65)
	(ULONG)FUNC(&IoCreateFile),                            // 0x0042 (66)
	(ULONG)FUNC(&IoCreateSymbolicLink),                    // 0x0043 (67)
	(ULONG)FUNC(&IoDeleteDevice),                          // 0x0044 (68)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&IoDeleteSymbolicLink),                    // 0x0045 (69)
	(ULONG)VARIABLE(&IoDeviceObjectType),                  // 0x0046 (70)
	(ULONG)VARIABLE(&IoFileObjectType),                    // 0x0047 (71)
	(ULONG)FUNC(&IoFreeIrp),                               // 0x0048 (72)
	(ULONG)FUNC(&IoInitializeIrp),                         // 0x0049 (73)
	(ULONG)FUNC(&IoInvalidDeviceRequest),                  // 0x004A (74)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&IoQueryFileInformation),                  // 0x004B (75)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&IoQueryVolumeInformation),                // 0x004C (76)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&IoQueueThreadIrp),                        // 0x004D (77)
	(ULONG)FUNC(&IoRemoveShareAccess),                     // 0x004E (78)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&IoSetIoCompletion),                       // 0x004F (79)
	(ULONG)FUNC(&IoSetShareAccess),                        // 0x0050 (80)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&IoStartNextPacket),                       // 0x0051 (81)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&IoStartNextPacketByKey),                  // 0x0052 (82)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&IoStartPacket),                           // 0x0053 (83)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&IoSynchronousDeviceIoControlRequest),     // 0x0054 (84)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&IoSynchronousFsdRequest),                 // 0x0055 (85)
	(ULONG)FUNC(&IofCallDriver),                           // 0x0056 (86)
	(ULONG)FUNC(&IofCompleteRequest),                      // 0x0057 (87)
	(ULONG)FUNC(nullptr), //(ULONG)VARIABLE(&KdDebuggerEnabled),                   // 0x0058 (88)
	(ULONG)FUNC(nullptr), //(ULONG)VARIABLE(&KdDebuggerNotPresent),                // 0x0059 (89)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&IoDismountVolume),                        // 0x005A (90)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&IoDismountVolumeByName),                  // 0x005B (91)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeAlertResumeThread),                     // 0x005C (92)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeAlertThread),                           // 0x005D (93)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeBoostPriorityThread),                   // 0x005E (94)
	(ULONG)FUNC(&KeBugCheck),                              // 0x005F (95)
	(ULONG)FUNC(&KeBugCheckEx),                            // 0x0060 (96)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeCancelTimer),                           // 0x0061 (97)
	(ULONG)FUNC(&KeConnectInterrupt),                      // 0x0062 (98)
	(ULONG)FUNC(&KeDelayExecutionThread),                  // 0x0063 (99)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeDisconnectInterrupt),                   // 0x0064 (100
	(ULONG)FUNC(&KeEnterCriticalRegion),                   // 0x0065 (101)
	(ULONG)VARIABLE(&MmGlobalData),                        // 0x0066 (102)
	(ULONG)FUNC(&KeGetCurrentIrql),                        // 0x0067 (103)
	(ULONG)FUNC(&KeGetCurrentThread),                      // 0x0068 (104)
	(ULONG)FUNC(&KeInitializeApc),                         // 0x0069 (105)
	(ULONG)FUNC(&KeInitializeDeviceQueue),                 // 0x006A (106)
	(ULONG)FUNC(&KeInitializeDpc),                         // 0x006B (107)
	(ULONG)FUNC(&KeInitializeEvent),                       // 0x006C (108)
	(ULONG)FUNC(&KeInitializeInterrupt),                   // 0x006D (109)
	(ULONG)FUNC(&KeInitializeMutant),                      // 0x006E (110)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeInitializeQueue),                       // 0x006F (111)
	(ULONG)FUNC(&KeInitializeSemaphore),                   // 0x0070 (112)
	(ULONG)FUNC(&KeInitializeTimerEx),                     // 0x0071 (113)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeInsertByKeyDeviceQueue),                // 0x0072 (114)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeInsertDeviceQueue),                     // 0x0073 (115)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeInsertHeadQueue),                       // 0x0074 (116)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeInsertQueue),                           // 0x0075 (117)
	(ULONG)FUNC(&KeInsertQueueApc),                        // 0x0076 (118)
	(ULONG)FUNC(&KeInsertQueueDpc),                        // 0x0077 (119)
	(ULONG)VARIABLE(&KeInterruptTime),                     // 0x0078 (120) KeInterruptTime
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeIsExecutingDpc),                        // 0x0079 (121)
	(ULONG)FUNC(&KeLeaveCriticalRegion),                   // 0x007A (122)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KePulseEvent),                            // 0x007B (123)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeQueryBasePriorityThread),               // 0x007C (124)
	(ULONG)FUNC(&KeQueryInterruptTime),                    // 0x007D (125)
	(ULONG)FUNC(&KeQueryPerformanceCounter),               // 0x007E (126)
	(ULONG)FUNC(&KeQueryPerformanceFrequency),             // 0x007F (127)
	(ULONG)FUNC(&KeQuerySystemTime),                       // 0x0080 (128)
	(ULONG)FUNC(&KeRaiseIrqlToDpcLevel),                   // 0x0081 (129)
	(ULONG)FUNC(&KeRaiseIrqlToSynchLevel),                 // 0x0082 (130)
	(ULONG)FUNC(&KeReleaseMutant),                         // 0x0083 (131)
	(ULONG)FUNC(&KeReleaseSemaphore),                      // 0x0084 (132)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeRemoveByKeyDeviceQueue),                // 0x0085 (133)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeRemoveDeviceQueue),                     // 0x0086 (134)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeRemoveEntryDeviceQueue),                // 0x0087 (135)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeRemoveQueue),                           // 0x0088 (136)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeRemoveQueueDpc),                        // 0x0089 (137)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeResetEvent),                            // 0x008A (138)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeRestoreFloatingPointState),             // 0x008B (139)
	(ULONG)FUNC(&KeResumeThread),                          // 0x008C (140)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeRundownQueue),                          // 0x008D (141)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeSaveFloatingPointState),                // 0x008E (142)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeSetBasePriorityThread),                 // 0x008F (143)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeSetDisableBoostThread),                 // 0x0090 (144)
	(ULONG)FUNC(&KeSetEvent),                              // 0x0091 (145)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeSetEventBoostPriority),                 // 0x0092 (146)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeSetPriorityProcess),                    // 0x0093 (147)
	(ULONG)FUNC(&KeSetPriorityThread),                     // 0x0094 (148)
	(ULONG)FUNC(&KeSetTimer),                              // 0x0095 (149)
	(ULONG)FUNC(&KeSetTimerEx),                            // 0x0096 (150)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeStallExecutionProcessor),               // 0x0097 (151)
	(ULONG)FUNC(&KeSuspendThread),                         // 0x0098 (152)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeSynchronizeExecution),                  // 0x0099 (153)
	(ULONG)VARIABLE(&KeSystemTime),                        // 0x009A (154) KeSystemTime
	(ULONG)FUNC(&KeTestAlertThread),                       // 0x009B (155)
	(ULONG)VARIABLE(&KeTickCount),                         // 0x009C (156)
	(ULONG)VARIABLE(&KeTimeIncrement),                     // 0x009D (157)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&KeWaitForMultipleObjects),                // 0x009E (158)
	(ULONG)FUNC(&KeWaitForSingleObject),                   // 0x009F (159)
	(ULONG)FUNC(&KfRaiseIrql),                             // 0x00A0 (160)
	(ULONG)FUNC(&KfLowerIrql),                             // 0x00A1 (161)
	(ULONG)FUNC(nullptr), //(ULONG)VARIABLE(&KiBugCheckData),                      // 0x00A2 (162)
	(ULONG)FUNC(&KiUnlockDispatcherDatabase),              // 0x00A3 (163)
	(ULONG)VARIABLE(&LaunchDataPage),                      // 0x00A4 (164)
	(ULONG)FUNC(&MmAllocateContiguousMemory),              // 0x00A5 (165)
	(ULONG)FUNC(&MmAllocateContiguousMemoryEx),            // 0x00A6 (166)
	(ULONG)FUNC(&MmAllocateSystemMemory),                  // 0x00A7 (167)
	(ULONG)FUNC(&MmClaimGpuInstanceMemory),                // 0x00A8 (168)
	(ULONG)FUNC(&MmCreateKernelStack),                     // 0x00A9 (169)
	(ULONG)FUNC(&MmDeleteKernelStack),                     // 0x00AA (170)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&MmFreeContiguousMemory),                  // 0x00AB (171)
	(ULONG)FUNC(&MmFreeSystemMemory),                      // 0x00AC (172)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&MmGetPhysicalAddress),                    // 0x00AD (173)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&MmIsAddressValid),                        // 0x00AE (174)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&MmLockUnlockBufferPages),                 // 0x00AF (175)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&MmLockUnlockPhysicalPage),                // 0x00B0 (176)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&MmMapIoSpace),                            // 0x00B1 (177)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&MmPersistContiguousMemory),               // 0x00B2 (178)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&MmQueryAddressProtect),                   // 0x00B3 (179)
	(ULONG)FUNC(&MmQueryAllocationSize),                   // 0x00B4 (180)
	(ULONG)FUNC(&MmQueryStatistics),                       // 0x00B5 (181)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&MmSetAddressProtect),                     // 0x00B6 (182)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&MmUnmapIoSpace),                          // 0x00B7 (183)
	(ULONG)FUNC(&NtAllocateVirtualMemory),                 // 0x00B8 (184)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtCancelTimer),                           // 0x00B9 (185)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtClearEvent),                            // 0x00BA (186)
	(ULONG)FUNC(&NtClose),                                 // 0x00BB (187)
	(ULONG)FUNC(&NtCreateDirectoryObject),                 // 0x00BC (188)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtCreateEvent),                           // 0x00BD (189)
	(ULONG)FUNC(&NtCreateFile),                            // 0x00BE (190)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtCreateIoCompletion),                    // 0x00BF (191)
	(ULONG)FUNC(&NtCreateMutant),                          // 0x00C0 (192)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtCreateSemaphore),                       // 0x00C1 (193)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtCreateTimer),                           // 0x00C2 (194)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtDeleteFile),                            // 0x00C3 (195)
	(ULONG)FUNC(&NtDeviceIoControlFile),                   // 0x00C4 (196)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtDuplicateObject),                       // 0x00C5 (197)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtFlushBuffersFile),                      // 0x00C6 (198)
	(ULONG)FUNC(&NtFreeVirtualMemory),                     // 0x00C7 (199)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtFsControlFile),                         // 0x00C8 (200)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtOpenDirectoryObject),                   // 0x00C9 (201)
	(ULONG)FUNC(&NtOpenFile),                              // 0x00CA (202)
	(ULONG)FUNC(&NtOpenSymbolicLinkObject),                // 0x00CB (203)
	(ULONG)FUNC(&NtProtectVirtualMemory),                  // 0x00CC (204)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtPulseEvent),                            // 0x00CD (205)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtQueueApcThread),                        // 0x00CE (206)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtQueryDirectoryFile),                    // 0x00CF (207)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtQueryDirectoryObject),                  // 0x00D0 (208)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtQueryEvent),                            // 0x00D1 (209)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtQueryFullAttributesFile),               // 0x00D2 (210)
	(ULONG)FUNC(&NtQueryInformationFile),                  // 0x00D3 (211)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtQueryIoCompletion),                     // 0x00D4 (212)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtQueryMutant),                           // 0x00D5 (213)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtQuerySemaphore),                        // 0x00D6 (214)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtQuerySymbolicLinkObject),               // 0x00D7 (215)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtQueryTimer),                            // 0x00D8 (216)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtQueryVirtualMemory),                    // 0x00D9 (217)
	(ULONG)FUNC(&NtQueryVolumeInformationFile),            // 0x00DA (218)
	(ULONG)FUNC(&NtReadFile),                              // 0x00DB (219)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtReadFileScatter),                       // 0x00DC (220)
	(ULONG)FUNC(&NtReleaseMutant),                         // 0x00DD (221)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtReleaseSemaphore),                      // 0x00DE (222)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtRemoveIoCompletion),                    // 0x00DF (223)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtResumeThread),                          // 0x00E0 (224)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtSetEvent),                              // 0x00E1 (225)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtSetInformationFile),                    // 0x00E2 (226)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtSetIoCompletion),                       // 0x00E3 (227)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtSetSystemTime),                         // 0x00E4 (228)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtSetTimerEx),                            // 0x00E5 (229)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtSignalAndWaitForSingleObjectEx),        // 0x00E6 (230)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtSuspendThread),                         // 0x00E7 (231)
	(ULONG)FUNC(&NtUserIoApcDispatcher),                   // 0x00E8 (232)
	(ULONG)FUNC(&NtWaitForSingleObject),                   // 0x00E9 (233)
	(ULONG)FUNC(&NtWaitForSingleObjectEx),                 // 0x00EA (234)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtWaitForMultipleObjectsEx),              // 0x00EB (235)
	(ULONG)FUNC(&NtWriteFile),                             // 0x00EC (236)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtWriteFileGather),                       // 0x00ED (237)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&NtYieldExecution),                        // 0x00EE (238)
	(ULONG)FUNC(&ObCreateObject),                          // 0x00EF (239)
	(ULONG)VARIABLE(&ObDirectoryObjectType),               // 0x00F0 (240)
	(ULONG)FUNC(&ObInsertObject),                          // 0x00F1 (241)
	(ULONG)FUNC(&ObMakeTemporaryObject),                   // 0x00F2 (242)
	(ULONG)FUNC(&ObOpenObjectByName),                      // 0x00F3 (243)
	(ULONG)FUNC(&ObOpenObjectByPointer),                   // 0x00F4 (244)
	(ULONG)VARIABLE(&ObpObjectHandleTable),                // 0x00F5 (245)
	(ULONG)FUNC(&ObReferenceObjectByHandle),               // 0x00F6 (246)
	(ULONG)FUNC(&ObReferenceObjectByName),                 // 0x00F7 (247)
	(ULONG)FUNC(&ObReferenceObjectByPointer),              // 0x00F8 (248)
	(ULONG)VARIABLE(&ObSymbolicLinkObjectType),            // 0x00F9 (249)
	(ULONG)FUNC(&ObfDereferenceObject),                    // 0x00FA (250)
	(ULONG)FUNC(&ObfReferenceObject),                      // 0x00FB (251)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&PhyGetLinkState),                         // 0x00FC (252)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&PhyInitialize),                           // 0x00FD (253)
	(ULONG)FUNC(&PsCreateSystemThread),                    // 0x00FE (254)
	(ULONG)FUNC(&PsCreateSystemThreadEx),                  // 0x00FF (255)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&PsQueryStatistics),                       // 0x0100 (256)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&PsSetCreateThreadNotifyRoutine),          // 0x0101 (257)
	(ULONG)FUNC(&PsTerminateSystemThread),                 // 0x0102 (258)
	(ULONG)VARIABLE(&PsThreadObjectType),                  // 0x0103 (259)
	(ULONG)FUNC(&RtlAnsiStringToUnicodeString),            // 0x0104 (260)
	(ULONG)FUNC(&RtlAppendStringToString),                 // 0x0105 (261)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlAppendUnicodeStringToString),          // 0x0106 (262)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlAppendUnicodeToString),                // 0x0107 (263)
	(ULONG)FUNC(&RtlAssert),                               // 0x0108 (264)
	(ULONG)FUNC(&RtlCaptureContext),                       // 0x0109 (265)
	(ULONG)FUNC(&RtlCaptureStackBackTrace),                // 0x010A (266)
	(ULONG)FUNC(&RtlCharToInteger),                        // 0x010B (267)
	(ULONG)FUNC(&RtlCompareMemory),                        // 0x010C (268)
	(ULONG)FUNC(&RtlCompareMemoryUlong),                   // 0x010D (269)
	(ULONG)FUNC(&RtlCompareString),                        // 0x010E (270)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlCompareUnicodeString),                 // 0x010F (271)
	(ULONG)FUNC(&RtlCopyString),                           // 0x0110 (272)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlCopyUnicodeString),                    // 0x0111 (273)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlCreateUnicodeString),                  // 0x0112 (274)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlDowncaseUnicodeChar),                  // 0x0113 (275)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlDowncaseUnicodeString),                // 0x0114 (276)
	(ULONG)FUNC(&RtlEnterCriticalSection),                 // 0x0115 (277)
	(ULONG)FUNC(&RtlEnterCriticalSectionAndRegion),        // 0x0116 (278)
	(ULONG)FUNC(&RtlEqualString),                          // 0x0117 (279)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlEqualUnicodeString),                   // 0x0118 (280)
	(ULONG)FUNC(&RtlExtendedIntegerMultiply),              // 0x0119 (281)
	(ULONG)FUNC(&RtlExtendedLargeIntegerDivide),           // 0x011A (282)
	(ULONG)FUNC(&RtlExtendedMagicDivide),                  // 0x011B (283)
	(ULONG)FUNC(&RtlFillMemory),                           // 0x011C (284)
	(ULONG)FUNC(&RtlFillMemoryUlong),                      // 0x011D (285)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlFreeAnsiString),                       // 0x011E (286)
	(ULONG)FUNC(&RtlFreeUnicodeString),                    // 0x011F (287)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlGetCallersAddress),                    // 0x0120 (288)
	(ULONG)FUNC(&RtlInitAnsiString),                       // 0x0121 (289)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlInitUnicodeString),                    // 0x0122 (290)
	(ULONG)FUNC(&RtlInitializeCriticalSection),            // 0x0123 (291)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlIntegerToChar),                        // 0x0124 (292)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlIntegerToUnicodeString),               // 0x0125 (293)
	(ULONG)FUNC(&RtlLeaveCriticalSection),                 // 0x0126 (294)
	(ULONG)FUNC(&RtlLeaveCriticalSectionAndRegion),        // 0x0127 (295)
	(ULONG)FUNC(&RtlLowerChar),                            // 0x0128 (296)
	(ULONG)FUNC(&RtlMapGenericMask),                       // 0x0129 (297)
	(ULONG)FUNC(&RtlMoveMemory),                           // 0x012A (298)
	(ULONG)FUNC(&RtlMultiByteToUnicodeN),                  // 0x012B (299)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlMultiByteToUnicodeSize),               // 0x012C (300)
	(ULONG)FUNC(&RtlNtStatusToDosError),                   // 0x012D (301)
	(ULONG)FUNC(&RtlRaiseException),                       // 0x012E (302)
	(ULONG)FUNC(&RtlRaiseStatus),                          // 0x012F (303)
	(ULONG)FUNC(&RtlTimeFieldsToTime),                     // 0x0130 (304)
	(ULONG)FUNC(&RtlTimeToTimeFields),                     // 0x0131 (305)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlTryEnterCriticalSection),              // 0x0132 (306)
	(ULONG)FUNC(&RtlUlongByteSwap),                        // 0x0133 (307)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlUnicodeStringToAnsiString),            // 0x0134 (308)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlUnicodeStringToInteger),               // 0x0135 (309)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlUnicodeToMultiByteN),                  // 0x0136 (310)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlUnicodeToMultiByteSize),               // 0x0137 (311)
	(ULONG)FUNC(&RtlUnwind),                               // 0x0138 (312)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlUpcaseUnicodeChar),                    // 0x0139 (313)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlUpcaseUnicodeString),                  // 0x013A (314)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlUpcaseUnicodeToMultiByteN),            // 0x013B (315)
	(ULONG)FUNC(&RtlUpperChar),                            // 0x013C (316)
	(ULONG)FUNC(&RtlUpperString),                          // 0x013D (317)
	(ULONG)FUNC(&RtlUshortByteSwap),                       // 0x013E (318)
	(ULONG)FUNC(&RtlWalkFrameChain),                       // 0x013F (319)
	(ULONG)FUNC(&RtlZeroMemory),                           // 0x0140 (320)
	(ULONG)VARIABLE(&XboxEEPROMKey),                       // 0x0141 (321)
	(ULONG)VARIABLE(&XboxHardwareInfo),                    // 0x0142 (322)
	(ULONG)FUNC(nullptr), //(ULONG)VARIABLE(&XboxHDKey),                           // 0x0143 (323)
	(ULONG)VARIABLE(&XboxKrnlVersion),                     // 0x0144 (324)
	(ULONG)FUNC(nullptr), //(ULONG)VARIABLE(&XboxSignatureKey),                    // 0x0145 (325)
	(ULONG)VARIABLE(&XeImageFileName),                     // 0x0146 (326)
	(ULONG)FUNC(&XeLoadSection),                           // 0x0147 (327)
	(ULONG)FUNC(&XeUnloadSection),                         // 0x0148 (328)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&READ_PORT_BUFFER_UCHAR),                  // 0x0149 (329)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&READ_PORT_BUFFER_USHORT),                 // 0x014A (330)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&READ_PORT_BUFFER_ULONG),                  // 0x014B (331)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&WRITE_PORT_BUFFER_UCHAR),                 // 0x014C (332)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&WRITE_PORT_BUFFER_USHORT),                // 0x014D (333)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&WRITE_PORT_BUFFER_ULONG),                 // 0x014E (334)
	(ULONG)FUNC(&XcSHAInit),                               // 0x014F (335)
	(ULONG)FUNC(&XcSHAUpdate),                             // 0x0150 (336)
	(ULONG)FUNC(&XcSHAFinal),                              // 0x0151 (337)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&XcRC4Key),                                // 0x0152 (338)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&XcRC4Crypt),                              // 0x0153 (339)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&XcHMAC),                                  // 0x0154 (340)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&XcPKEncPublic),                           // 0x0155 (341)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&XcPKDecPrivate),                          // 0x0156 (342)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&XcPKGetKeyLen),                           // 0x0157 (343)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&XcVerifyPKCS1Signature),                  // 0x0158 (344)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&XcModExp),                                // 0x0159 (345)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&XcDESKeyParity),                          // 0x015A (346)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&XcKeyTable),                              // 0x015B (347)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&XcBlockCrypt),                            // 0x015C (348)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&XcBlockCryptCBC),                         // 0x015D (349)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&XcCryptService),                          // 0x015E (350)
	(ULONG)FUNC(&XcUpdateCrypto),                          // 0x015F (351)
	(ULONG)FUNC(&RtlRip),                                  // 0x0160 (352)
	(ULONG)FUNC(nullptr), //(ULONG)VARIABLE(&XboxLANKey),                          // 0x0161 (353)
	(ULONG)FUNC(nullptr), //(ULONG)VARIABLE(&XboxAlternateSignatureKeys),          // 0x0162 (354)
	(ULONG)FUNC(nullptr), //(ULONG)VARIABLE(&XePublicKeyData),                     // 0x0163 (355)
	(ULONG)VARIABLE(&HalBootSMCVideoMode),                 // 0x0164 (356)
	(ULONG)FUNC(nullptr), //(ULONG)VARIABLE(&IdexChannelObject),                   // 0x0165 (357)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&HalIsResetOrShutdownPending),             // 0x0166 (358)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&IoMarkIrpMustComplete),                   // 0x0167 (359)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&HalInitiateShutdown),                     // 0x0168 (360)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlSnprintf),                             // 0x0169 (361)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlSprintf),                              // 0x016A (362)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlVsnprintf),                            // 0x016B (363) 
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&RtlVsprintf),                             // 0x016C (364)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&HalEnableSecureTrayEject),                // 0x016D (365)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&HalWriteSMCScratchRegister),              // 0x016E (366)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&UnknownAPI367),                           // 0x016F (367)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&UnknownAPI368),                           // 0x0170 (368)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&UnknownAPI369),                           // 0x0171 (369)
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&XProfpControl),                           // 0x0172 (370) PROFILING
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&XProfpGetData),                           // 0x0173 (371) PROFILING
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&IrtClientInitFast),                       // 0x0174 (372) PROFILING
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&IrtSweep),                                // 0x0175 (373) PROFILING
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&MmDbgAllocateMemory),                     // 0x0176 (374) DEVKIT ONLY!
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&MmDbgFreeMemory),                         // 0x0177 (375) DEVKIT ONLY!
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&MmDbgQueryAvailablePages),                // 0x0178 (376) DEVKIT ONLY!
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&MmDbgReleaseAddress),                     // 0x0179 (377) DEVKIT ONLY!
	(ULONG)FUNC(nullptr), //(ULONG)FUNC(&MmDbgWriteCheck),                         // 0x017A (378) DEVKIT ONLY!
};
