/*
* SPDX-License-Identifier: GPL-2.0+
*
* ergo720                Copyright (c) 2026
*
* This program is free software; you can redistribute it
* and/or modify it under the terms of the GNU General Public
* License as published by the Free Software Foundation; either
* version 2 of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#pragma once

// CMOS I/O ports
#define CMOS_PORT_CMD 0x70
#define CMOS_PORT_DATA 0x71

// PIT I/O ports
#define PIT_CHANNEL0_DATA  0x40
#define PIT_PORT_CMD       0x43

// PIT initialization commands
#define PIT_COUNT_BINARY   0x00
#define PIT_COUNT_MODE     0x04
#define PIT_COUNT_16BIT    0x30
#define PIT_COUNT_CHAN0    0x00

// PIT counter required to trigger IRQ0 every one ms
// NOTE: on the xbox, the pit frequency is 6% lower than the default one, see https://xboxdevwiki.net/Porting_an_Operating_System_to_the_Xbox_HOWTO#Timer_Frequency
#define PIT_COUNTER_1MS    1125

// PIC i/o ports
#define PIC_MASTER_CMD          0x20
#define PIC_MASTER_DATA         0x21
#define PIC_MASTER_ELCR         0x4D0
#define PIC_SLAVE_CMD           0xA0
#define PIC_SLAVE_DATA          0xA1
#define PIC_SLAVE_ELCR          0x4D1

// PIC icw1 command bytes
#define ICW1_ICW4_NEEDED        0x01
#define ICW1_CASCADE            0x00
#define ICW1_INTERVAL8          0x00
#define ICW1_EDGE               0x00
#define ICW1_INIT               0x10

// PIC icw4 command bytes
#define ICW4_8086               0x01
#define ICW4_NORNAL_EOI         0x00
#define ICW4_NON_BUFFERED       0x00
#define ICW4_NOT_FULLY_NESTED   0x00

// PIC ocw2 command bytes
#define OCW2_EOI_IRQ            0x60

// PIC ocw3 command bytes
#define OCW3_READ_ISR           0x0B

// PIC irq base (icw2)
#define PIC_MASTER_VECTOR_BASE  IDT_INT_VECTOR_BASE
#define PIC_SLAVE_VECTOR_BASE   (IDT_INT_VECTOR_BASE + 8)

// PCI ports
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

// LPC bridge/ACPI ports, registers and values
#define LPCBRIDGE_IO_REGISTER_BASE 0x8000
#define ACPI_GPE0_STATUS_REGISTER  0x20
#define ACPI_GPE0_EXTSMI_STATUS    0x02

// Host bridge PCI registers and values
#define CPU_MAX_MEM        0x84
#define CPU_MAX_MEM_64MB   0x03
#define CPU_MAX_MEM_128MB  0x07
#define CPU_MAX_MEM_256MB  0x0F
#define CPU_MAX_MEM_RESV   0xFFFFFF

// SMBUS ports
#define SMBUS_STATUS 0xC000
#define SMBUS_CONTROL 0xC002
#define SMBUS_ADDRESS 0xC004
#define SMBUS_DATA 0xC006
#define SMBUS_COMMAND 0xC008
#define SMBUS_FIFO 0xC009

// SMBUS constants
#define GS_PRERR_STS (1 << 2) // cycle failed
#define GS_HCYC_STS (1 << 4) // cycle succeeded
#define GE_RW_BYTE 2
#define GE_RW_WORD 3
#define GE_RW_BLOCK 5
#define GE_HOST_STC (1 << 3) // set to start a cycle
#define GE_HCYC_EN (1 << 4) // set to enable interrupts
#define HA_RC (1 << 0) // set to specify a read/receive cycle

// SMBUS sw addresses
#define EEPROM_WRITE_ADDR 0xA8
#define EEPROM_READ_ADDR 0xA9
#define SMC_WRITE_ADDR 0x20
#define SMC_READ_ADDR 0x21
#define CONEXANT_WRITE_ADDR 0x8A
#define CONEXANT_READ_ADDR 0x8B

// SMC commands
#define SMC_COMMAND_TRAY_STATE        0x03
#define SMC_COMMAND_VIDEO_MODE        0x04
#define SMC_COMMAND_INTERRUPT_REASON  0x11
#define SMC_COMMAND_INTERRUPT_ENABLE  0x1A

// SMC video mode values
#define SMC_VIDEO_MODE_SCART     0x00
#define SMC_VIDEO_MODE_HDTV      0x01
#define SMC_VIDEO_MODE_VGA       0x02
#define SMC_VIDEO_MODE_RFU       0x03
#define SMC_VIDEO_MODE_SVIDEO    0x04
#define SMC_VIDEO_MODE_STANDARD  0x06
#define SMC_VIDEO_MODE_NONE      0x07

// SMC tray state values
#define SMC_TRAY_STATE_ACTIVITY      0x01
#define SMC_TRAY_STATE_STATE_MASK    0x70
#define SMC_TRAY_STATE_CLOSED        0x00
#define SMC_TRAY_STATE_OPEN          0x10
#define SMC_TRAY_STATE_UNLOADING     0x20
#define SMC_TRAY_STATE_OPENING       0x30
#define SMC_TRAY_STATE_NO_MEDIA      0x40
#define SMC_TRAY_STATE_CLOSING       0x50
#define SMC_TRAY_STATE_MEDIA_DETECT  0x60
#define SMC_TRAY_STATE_RESET         0x70

// SMC interrupt reason values
#define SMC_INTERRUPT_REASON_SHUTDOWN       0x01
#define SMC_INTERRUPT_REASON_TRAY_DETECTED  0x02
#define SMC_INTERRUPT_REASON_TRAY_OPENED    0x04
#define SMC_INTERRUPT_REASON_NEW_AV_PACK    0x08
#define SMC_INTERRUPT_REASON_NO_AV_PACK     0x10
#define SMC_INTERRUPT_REASON_TRAY_EJECT     0x20
#define SMC_INTERRUPT_REASON_TRAY_CLOSED    0x40
