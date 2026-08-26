/*****************************************************************************\
	 Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
				This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

#include "snes9x.h"
#include "memmap.h"
#include "fxinst.h"
#include "fxemu.h"

static void FxReset(struct FxInfo_s *);
static void fx_readRegisterSpace(void);
static void fx_writeRegisterSpace(void);
static void fx_updateRamBank(uint8);
static void fx_dirtySCBR(void);
static bool8 fx_checkStartAddress(void);
static uint32 FxEmulate(uint32);
static void FxCacheWriteAccess(uint16);
static void FxFlushCache(void);

void S9xInitSuperFX(void) {
	memset((uint8 *)&GSU, 0, sizeof(struct FxRegs_s));
}

void S9xResetSuperFX(void) {
	// FIXME: Snes9x only runs the SuperFX at the end of every line.
	// 5823405 is a magic number that seems to work for most games.
	SuperFX.speedPerLine = (uint32)(5823405 * ((1.0 / (float)Memory.ROMFramesPerSecond) / ((float)(Timings.V_Max))));
	SuperFX.oneLineDone = FALSE;
	SuperFX.vFlags = 0;
	CPU.IRQExternal = FALSE;
	FxReset(&SuperFX);
	if (SuperFX.isFx4) {
		Memory.Map_SuperFX4RomWindows();
	}
}

void S9xSetSuperFX(uint8 byte, uint16 address) {
	switch (address) {
	case 0x3030:
		if ((Memory.FillRAM[0x3030] ^ byte) & FLG_G) {
			Memory.FillRAM[0x3030] = byte;
			if (byte & FLG_G) {
				if (!SuperFX.oneLineDone) {
					S9xSuperFXExec();
					SuperFX.oneLineDone = TRUE;
				}
			} else {
				FxFlushCache();
			}
		} else {
			Memory.FillRAM[0x3030] = byte;
		}

		break;

	case 0x3031:
		Memory.FillRAM[0x3031] = byte;
		break;

	case 0x3032:
		Memory.FillRAM[0x3032] = byte;
		if (SuperFX.isFx4) {
			Memory.Map_SuperFX4RomWindows();
		}
		break;

	case 0x3033:
		Memory.FillRAM[0x3033] = byte;
		break;

	case 0x3034:
		Memory.FillRAM[0x3034] = SuperFX.isFx4 ? byte : (byte & 0x7f);
		break;

	case 0x3035:
		Memory.FillRAM[0x3035] = byte;
		if (SuperFX.isFx4) {
			Memory.Map_SuperFX4RomWindows();
		}
		break;

	case 0x3036:
		Memory.FillRAM[0x3036] = SuperFX.isFx4 ? byte : (byte & 0x7f);
		break;

	case 0x3037:
		Memory.FillRAM[0x3037] = byte;
		break;

	case 0x3038:
		Memory.FillRAM[0x3038] = byte;
		fx_dirtySCBR();
		break;

	case 0x3039:
		Memory.FillRAM[0x3039] = byte;
		break;

	case 0x303a:
		Memory.FillRAM[0x303a] = byte;
		break;

	case 0x303b:
		break;

	case 0x303c:
		Memory.FillRAM[0x303c] = byte;
		fx_updateRamBank(byte);
		break;

	case 0x303d:
		Memory.FillRAM[0x303d] = byte;
		if (SuperFX.isFx4) {
			fx_rebuildFx4RomBanks();
		}
		break;

	case 0x303f:
		Memory.FillRAM[0x303f] = byte;
		break;

	case 0x301f:
		Memory.FillRAM[0x301f] = byte;
		Memory.FillRAM[0x3000 + GSU_SFR] |= FLG_G;
		if (!SuperFX.oneLineDone) {
			S9xSuperFXExec();
			SuperFX.oneLineDone = TRUE;
		}

		break;

	default:
		Memory.FillRAM[address] = byte;
		if (SuperFX.isFx4 ? (address >= 0x3040 && address <= 0x3fff)
						  : (address >= 0x3100)) {
			FxCacheWriteAccess(address);
		}

		break;
	}
}

uint8 S9xGetSuperFX(uint16 address) {
	uint8 byte;

	byte = Memory.FillRAM[address];

	if (address == 0x3031) {
		CPU.IRQExternal = FALSE;
		Memory.FillRAM[0x3031] = byte & 0x7f;
	}

	return (byte);
}

void S9xSuperFXExec(void) {
	// FX3/FX4: the CPU keeps ROM/RAM access, so SCMR RON/RAN don't gate execution
	if ((Memory.FillRAM[0x3000 + GSU_SFR] & FLG_G) &&
		(SuperFX.isFx3 || SuperFX.isFx4 || (Memory.FillRAM[0x3000 + GSU_SCMR] & 0x18) != 0)) {
		int clockMul = (SuperFX.isFx3 || SuperFX.isFx4) ? 4 : 1;
		FxEmulate(((Memory.FillRAM[0x3000 + GSU_CLSR] & 1) ? (SuperFX.speedPerLine * 5 / 2) : SuperFX.speedPerLine) * clockMul * Settings.SuperFXClockMultiplier / 100);

		uint16 GSUStatus = Memory.FillRAM[0x3000 + GSU_SFR] | (Memory.FillRAM[0x3000 + GSU_SFR + 1] << 8);
		if ((GSUStatus & (FLG_G | FLG_IRQ)) == FLG_IRQ) {
			CPU.IRQExternal = TRUE;
		}
	}
}

static void FxReset(struct FxInfo_s *psFxInfo) {
	// Clear all internal variables
	memset((uint8 *)&GSU, 0, sizeof(struct FxRegs_s));

	// Set default registers
	GSU.pvSreg = GSU.pvDreg = &R0;

	// Set RAM and ROM pointers
	GSU.pvRegisters = psFxInfo->pvRegisters;
	GSU.nRamBanks = psFxInfo->nRamBanks;
	GSU.pvRam = psFxInfo->pvRam;
	GSU.nRomBanks = psFxInfo->nRomBanks;
	GSU.pvRom = psFxInfo->pvRom;
	GSU.bFx3 = psFxInfo->isFx3;
	GSU.bFx4 = psFxInfo->isFx4;
	GSU.vPrevScreenHeight = ~0;
	GSU.vPrevMode = ~0;

	if (GSU.bFx4) {
		if (GSU.nRamBanks < FX4_MIN_RAM_BANKS) {
			GSU.nRamBanks = FX4_MIN_RAM_BANKS;
		}
		if (GSU.nRamBanks > FX4_RAM_BANKS) {
			GSU.nRamBanks = FX4_RAM_BANKS;
		}

		// Clear register file + 4KB cache window ($3000/$3FFF); $303D = 0
		memset(GSU.pvRegisters, 0, 0x1000);
		GSU.pvRegisters[0x3b] = 0x54; // VCR

		fx_rebuildFx4RomBanks();

		// Linear RAM banks (full SRAM)
		for (int i = 0; i < FX4_RAM_BANKS; i++) {
			GSU.apvRamBankFx4[i] = &GSU.pvRam[((uint32)i % GSU.nRamBanks) << 16];
		}

		// Keep classic 4-entry table in sync for any shared paths / snapshots
		for (int i = 0; i < FX_RAM_BANKS; i++) {
			GSU.apvRamBank[i] = GSU.apvRamBankFx4[i % GSU.nRamBanks];
		}

		GSU.pvCache = &GSU.pvRegisters[0x40]; // $3040
	} else {
		// The GSU can't access more than 2mb (16mbits); the FX3 sees 4mb
		uint32 maxBanks = GSU.bFx3 ? 0x40 : 0x20;
		if (GSU.nRomBanks > maxBanks) {
			GSU.nRomBanks = maxBanks;
		}

		// Clear FxChip register space
		memset(GSU.pvRegisters, 0, 0x300);

		// Set FxChip version Number
		GSU.pvRegisters[0x3b] = GSU.bFx3 ? 0x52 : 0;

		// Make ROM bank table (classic 32K mirrors for low banks)
		for (int i = 0; i < 256; i++) {
			uint32 b = i & 0x7f;

			if (b >= 0x40) {
				if (GSU.nRomBanks > 1) {
					b %= GSU.nRomBanks;
				} else {
					b &= 1;
				}

				GSU.apvRomBank[i] = &GSU.pvRom[b << 16];
			} else {
				b %= GSU.nRomBanks * 2;
				GSU.apvRomBank[i] = &GSU.pvRom[(b << 16) + FX_MEMORY_32K_MIRRORS];
			}
		}

		// Make RAM bank table
		for (int i = 0; i < 4; i++) {
			GSU.apvRamBank[i] = &GSU.pvRam[(i % GSU.nRamBanks) << 16];
			GSU.apvRomBank[0x70 + i] = GSU.apvRamBank[i];
		}

		GSU.pvCache = &GSU.pvRegisters[0x100];
	}

	// Start with a nop in the pipe
	GSU.vPipe = 0x01;

	fx_applyOpcodeTable();
	fx_readRegisterSpace();
}

void fx_rebuildFx4RomBanks(void) {
	uint8 *base = SuperFX.pvRom;
	uint32 size = Memory.CalculatedSize;

	GSU.pvRom = base;

	uint32 offset = 0;
	if (GSU.pvRegisters) {
		offset = (uint32)GSU.pvRegisters[GSU_FX4BSW3] * 0x100000u;
	}

	if (size == 0) {
		GSU.nRomBanks = 1;
		for (int i = 0; i < 256; i++) {
			GSU.apvRomBank[i] = Memory.ROM;
		}
	} else {
		if (offset >= size) {
			offset %= size;
		}

		uint8 *window = base + offset;
		uint32 avail = size - offset;
		GSU.nRomBanks = (avail + 0xffff) >> 16;
		if (GSU.nRomBanks < 1) {
			GSU.nRomBanks = 1;
		}
		if (GSU.nRomBanks > 256) {
			GSU.nRomBanks = 256;
		}

		uint8 *romEnd = Memory.ROM + Memory.ROMAllocSize();
		uint8 *safe = (Memory.ROMAllocSize() >= 0x10000) ? (romEnd - 0x10000) : Memory.ROM;

		for (int i = 0; i < 256; i++) {
			uint32 addr = Memory.map_mirror(avail, ((uint32)i % GSU.nRomBanks) << 16);
			uint8 *p = window + addr;
			if (p + 0x10000 > romEnd) {
				p = safe;
			}
			GSU.apvRomBank[i] = p;
		}
	}

	GSU.pvRomBank = GSU.apvRomBank[GSU.vRomBankReg & 0xff];
	GSU.pvPrgBank = GSU.apvRomBank[GSU.vPrgBankReg & 0xff];
}

static void fx_readRegisterSpace(void) {
	static const uint32 avHeight[] = {128, 160, 192, 256};
	static const uint32 avMult[] = {16, 32, 32, 64};

	uint8 *p;
	int n;

	GSU.vErrorCode = 0;

	// Update R0-R15
	p = GSU.pvRegisters;
	for (int i = 0; i < 16; i++, p += 2) {
		GSU.avReg[i] = (uint32)READ_WORD(p);
	}

	// Update other registers
	p = GSU.pvRegisters;
	GSU.vStatusReg = (uint32)READ_WORD(&p[GSU_SFR]);
	GSU.vPrgBankReg = (uint32)p[GSU_PBR];
	GSU.vRomBankReg = (uint32)p[GSU_ROMBR];
	GSU.vRamBankReg = GSU.bFx4 ? (uint32)p[GSU_RAMBR]
							   : ((uint32)p[GSU_RAMBR]) & (FX_RAM_BANKS - 1);
	GSU.vCacheBaseReg = (uint32)p[GSU_CBR];
	GSU.vCacheBaseReg |= ((uint32)p[GSU_CBR + 1]) << 8;

	// Update status register variables
	GSU.vZero = !(GSU.vStatusReg & FLG_Z);
	GSU.vSign = (GSU.vStatusReg & FLG_S) << 12;
	GSU.vOverflow = (GSU.vStatusReg & FLG_OV) << 16;
	GSU.vCarry = (GSU.vStatusReg & FLG_CY) >> 2;

	// Set bank pointers
	if (GSU.bFx4) {
		GSU.vRamBankReg &= 0xff;
		GSU.pvRamBank = GSU.apvRamBankFx4[GSU.vRamBankReg % (GSU.nRamBanks ? GSU.nRamBanks : 1)];
	} else {
		GSU.pvRamBank = GSU.apvRamBank[GSU.vRamBankReg & 0x3];
	}
	GSU.pvRomBank = GSU.apvRomBank[GSU.vRomBankReg & 0xff];
	GSU.pvPrgBank = GSU.apvRomBank[GSU.vPrgBankReg & 0xff];

	// Set screen pointers
	GSU.pvScreenBase = &GSU.pvRam[USEX8(p[GSU_SCBR]) << 10];
	n = (int)(!!(p[GSU_SCMR] & 0x04));
	n |= ((int)(!!(p[GSU_SCMR] & 0x20))) << 1;
	GSU.vScreenHeight = GSU.vScreenRealHeight = avHeight[n];
	GSU.vMode = p[GSU_SCMR] & 0x03;

	if (n == 3) {
		GSU.vScreenSize = (256 / 8) * (256 / 8) * 32;
	} else {
		GSU.vScreenSize = (GSU.vScreenHeight / 8) * (256 / 8) * avMult[GSU.vMode];
	}

	if (GSU.vPlotOptionReg & 0x10) { // OBJ Mode (for drawing into sprites)
		GSU.vScreenHeight = 256;
	}

	if (GSU.pvScreenBase + GSU.vScreenSize > GSU.pvRam + (GSU.nRamBanks * 65536)) {
		GSU.pvScreenBase = GSU.pvRam + (GSU.nRamBanks * 65536) - GSU.vScreenSize;
	}

	GSU.pfPlot = fx_PlotTable[GSU.vMode];
	GSU.pfRpix = fx_PlotTable[GSU.vMode + 5];

	fx_OpcodeTable[0x04c] = GSU.pfPlot;
	fx_OpcodeTable[0x14c] = GSU.pfRpix;
	fx_OpcodeTable[0x24c] = GSU.pfPlot;
	fx_OpcodeTable[0x34c] = GSU.pfRpix;

	fx_computeScreenPointers();

	// fx_backupCache();
}

static void fx_writeRegisterSpace(void) {
	uint8 *p;

	p = GSU.pvRegisters;
	for (int i = 0; i < 16; i++, p += 2) {
		WRITE_WORD(p, GSU.avReg[i]);
	}

	// Update status register
	if (USEX16(GSU.vZero) == 0) {
		SF(Z);
	} else {
		CF(Z);
	}

	if (GSU.vSign & 0x8000) {
		SF(S);
	} else {
		CF(S);
	}

	if (GSU.vOverflow >= 0x8000 || GSU.vOverflow < -0x8000) {
		SF(OV);
	} else {
		CF(OV);
	}

	if (GSU.vCarry) {
		SF(CY);
	} else {
		CF(CY);
	}

	p = GSU.pvRegisters;
	WRITE_WORD(&p[GSU_SFR], GSU.vStatusReg);
	p[GSU_PBR] = (uint8)GSU.vPrgBankReg;
	p[GSU_ROMBR] = (uint8)GSU.vRomBankReg;
	p[GSU_RAMBR] = (uint8)GSU.vRamBankReg;
	WRITE_WORD(&p[GSU_CBR], GSU.vCacheBaseReg);

	// fx_restoreCache();
}

// Update RamBankReg and RAM Bank pointer
static void fx_updateRamBank(uint8 byte) {
	if (GSU.bFx4) {
		GSU.vRamBankReg = (uint32)byte;
		GSU.pvRamBank = GSU.apvRamBankFx4[byte % (GSU.nRamBanks ? GSU.nRamBanks : 1)];
	} else {
		GSU.vRamBankReg = (uint32)byte & (FX_RAM_BANKS - 1);
		GSU.pvRamBank = GSU.apvRamBank[byte & 0x3];
	}
}

// SCBR write seen. We need to update our cached screen pointers
static void fx_dirtySCBR(void) {
	GSU.vSCBRDirty = TRUE;
}

static bool8 fx_checkStartAddress(void) {
	uint32 cacheSize = GSU.bFx4 ? FX4_CACHE_SIZE : 512;

	// Check if we start inside the cache
	if (GSU.bCacheActive && R15 >= GSU.vCacheBaseReg && R15 < (GSU.vCacheBaseReg + cacheSize)) {
		return true;
	}

	// FX4: no bus arbitration; full linear ROM space
	if (GSU.bFx4) {
		return true;
	}

	// FX3: no bus arbitration, any ROM (<= 0x6f) or RAM (0x70-0x71) bank runs
	if (GSU.bFx3) {
		return (GSU.vPrgBankReg <= 0x71);
	}

	if (SCMR & (1 << 4)) {
		if (GSU.vPrgBankReg <= 0x5f || GSU.vPrgBankReg >= 0x80) {
			return true;
		}
	}

	if (GSU.vPrgBankReg <= 0x7f && (SCMR & (1 << 3))) {
		return true;
	}

	return false;
}

// Execute until the next stop instruction
static uint32 FxEmulate(uint32 nInstructions) {
	uint32 vCount;

	// Read registers and initialize GSU session
	fx_readRegisterSpace();

	// Check if the start address is valid
	if (!fx_checkStartAddress()) {
		CF(G);
		fx_writeRegisterSpace();
		/*
		GSU.vIllegalAddress = (GSU.vPrgBankReg << 24) | R15;
		return (FX_ERROR_ILLEGAL_ADDRESS);
		*/

		return (0);
	}

	// Execute GSU session
	CF(IRQ);

	/*
	if (GSU.bBreakPoint)
		vCount = fx_run_to_breakpoint(nInstructions);
	else
	*/
	vCount = fx_run(nInstructions);

	// Store GSU registers
	fx_writeRegisterSpace();

	// Check for error code
	if (GSU.vErrorCode) {
		return (GSU.vErrorCode);
	} else {
		return (vCount);
	}
}

void fx_computeScreenPointers(void) {
	if (GSU.vMode != GSU.vPrevMode || GSU.vPrevScreenHeight != GSU.vScreenHeight || GSU.vSCBRDirty) {
		GSU.vSCBRDirty = FALSE;

		// Make a list of pointers to the start of each screen column
		switch (GSU.vScreenHeight) {
		case 128:
			switch (GSU.vMode) {
			case 0:
				for (int i = 0; i < 32; i++) {
					GSU.apvScreen[i] = GSU.pvScreenBase + (i << 4);
					GSU.x[i] = i << 8;
				}

				break;

			case 1:
				for (int i = 0; i < 32; i++) {
					GSU.apvScreen[i] = GSU.pvScreenBase + (i << 5);
					GSU.x[i] = i << 9;
				}

				break;

			case 2:
			case 3:
				for (int i = 0; i < 32; i++) {
					GSU.apvScreen[i] = GSU.pvScreenBase + (i << 6);
					GSU.x[i] = i << 10;
				}

				break;
			}

			break;

		case 160:
			switch (GSU.vMode) {
			case 0:
				for (int i = 0; i < 32; i++) {
					GSU.apvScreen[i] = GSU.pvScreenBase + (i << 4);
					GSU.x[i] = (i << 8) + (i << 6);
				}

				break;

			case 1:
				for (int i = 0; i < 32; i++) {
					GSU.apvScreen[i] = GSU.pvScreenBase + (i << 5);
					GSU.x[i] = (i << 9) + (i << 7);
				}

				break;

			case 2:
			case 3:
				for (int i = 0; i < 32; i++) {
					GSU.apvScreen[i] = GSU.pvScreenBase + (i << 6);
					GSU.x[i] = (i << 10) + (i << 8);
				}

				break;
			}

			break;

		case 192:
			switch (GSU.vMode) {
			case 0:
				for (int i = 0; i < 32; i++) {
					GSU.apvScreen[i] = GSU.pvScreenBase + (i << 4);
					GSU.x[i] = (i << 8) + (i << 7);
				}

				break;

			case 1:
				for (int i = 0; i < 32; i++) {
					GSU.apvScreen[i] = GSU.pvScreenBase + (i << 5);
					GSU.x[i] = (i << 9) + (i << 8);
				}

				break;

			case 2:
			case 3:
				for (int i = 0; i < 32; i++) {
					GSU.apvScreen[i] = GSU.pvScreenBase + (i << 6);
					GSU.x[i] = (i << 10) + (i << 9);
				}

				break;
			}

			break;

		case 256:
			switch (GSU.vMode) {
			case 0:
				for (int i = 0; i < 32; i++) {
					GSU.apvScreen[i] = GSU.pvScreenBase + ((i & 0x10) << 9) + ((i & 0xf) << 8);
					GSU.x[i] = ((i & 0x10) << 8) + ((i & 0xf) << 4);
				}

				break;

			case 1:
				for (int i = 0; i < 32; i++) {
					GSU.apvScreen[i] = GSU.pvScreenBase + ((i & 0x10) << 10) + ((i & 0xf) << 9);
					GSU.x[i] = ((i & 0x10) << 9) + ((i & 0xf) << 5);
				}

				break;

			case 2:
			case 3:
				for (int i = 0; i < 32; i++) {
					GSU.apvScreen[i] = GSU.pvScreenBase + ((i & 0x10) << 11) + ((i & 0xf) << 10);
					GSU.x[i] = ((i & 0x10) << 10) + ((i & 0xf) << 6);
				}

				break;
			}

			break;
		}

		GSU.vPrevMode = GSU.vMode;
		GSU.vPrevScreenHeight = GSU.vScreenHeight;
	}
}

// Write access to the cache
static void FxCacheWriteAccess(uint16 vAddress) {
	/*
	if (!GSU.bCacheActive)
	{
		uint8	v = GSU.pvCache[GSU.pvCache[vAddress & 0x1ff];
		fx_setCache();
		GSU.pvCache[GSU.pvCache[vAddress & 0x1ff] = v;
	}
	*/

	if ((vAddress & 0x00f) == 0x00f) {
		GSU.vCacheFlags |= 1 << ((vAddress & 0x1f0) >> 4);
	}
}

static void FxFlushCache(void) {
	GSU.vCacheFlags = 0;
	GSU.vCacheBaseReg = 0;
	GSU.bCacheActive = FALSE;
	// GSU.vPipe = 0x1;
}

void fx_flushCache(void) {
	// fx_restoreCache();
	GSU.vCacheFlags = 0;
	GSU.bCacheActive = FALSE;
}

/*
static void fx_setCache (void)
{
	uint32	c;

	GSU.bCacheActive = TRUE;
	GSU.pvRegisters[0x3e] &= 0xf0;

	c  =  (uint32) GSU.pvRegisters[0x3e];
	c |= ((uint32) GSU.pvRegisters[0x3f]) << 8;
	if (c == GSU.vCacheBaseReg)
		return;

	GSU.vCacheBaseReg = c;
	GSU.vCacheFlags = 0;

	if (c < (0x10000 - 512))
	{
		const uint8	*t = &ROM(c);
		memcpy(GSU.pvCache, t, 512);
	}
	else
	{
		const uint8	*t1, *t2;
		uint32		i = 0x10000 - c;

		t1 = &ROM(c);
		t2 = &ROM(0);
		memcpy(GSU.pvCache, t1, i);
		memcpy(&GSU.pvCache[i], t2, 512 - i);
	}
}
*/

/*
static void fx_backupCache (void)
{
	uint32	v = GSU.vCacheFlags;
	uint32	c = USEX16(GSU.vCacheBaseReg);

	if (v)
	{
		for (int i = 0; i < 32; i++)
		{
			if (v & 1)
			{
				if (c < (0x10000 - 16))
				{
					uint8	*t = &GSU.pvPrgBank[c];
					memcpy(&GSU.avCacheBackup[i << 4], t, 16);
					memcpy(t, &GSU.pvCache[i << 4], 16);
				}
				else
				{
					uint8	*t1, *t2;
					uint32	a = 0x10000 - c;

					t1 = &GSU.pvPrgBank[c];
					t2 = &GSU.pvPrgBank[0];
					memcpy(&GSU.avCacheBackup[i << 4], t1, a);
					memcpy(t1, &GSU.pvCache[i << 4], a);
					memcpy(&GSU.avCacheBackup[(i << 4) + a], t2, 16 - a);
					memcpy(t2, &GSU.pvCache[(i << 4) + a], 16 - a);
				}
			}

			c = USEX16(c + 16);
			v >>= 1;
		}
	}
}
*/

/*
static void fx_restoreCache()
{
	uint32	v = GSU.vCacheFlags;
	uint32	c = USEX16(GSU.vCacheBaseReg);

	if (v)
	{
		for (int i = 0; i < 32; i++)
		{
			if (v & 1)
			{
				if (c < (0x10000 - 16))
				{
					uint8	*t = &GSU.pvPrgBank[c];
					memcpy(t, &GSU.avCacheBackup[i << 4], 16);
					memcpy(&GSU.pvCache[i << 4], t, 16);
				}
				else
				{
					uint8	*t1, *t2;
					uint32	a = 0x10000 - c;

					t1 = &GSU.pvPrgBank[c];
					t2 = &GSU.pvPrgBank[0];
					memcpy(t1, &GSU.avCacheBackup[i << 4], a);
					memcpy(&GSU.pvCache[i << 4], t1, a);
					memcpy(t2, &GSU.avCacheBackup[(i << 4) + a], 16 - a);
					memcpy(&GSU.pvCache[(i << 4) + a], t2, 16 - a);
				}
			}

			c = USEX16(c + 16);
			v >>= 1;
		}
	}
}
*/

// Breakpoints
/*
static void FxBreakPointSet (uint32 vAddress)
{
	GSU.bBreakPoint = TRUE;
	GSU.vBreakPoint = USEX16(vAddress);
}
*/

/*
static void FxBreakPointClear (void)
{
	GSU.bBreakPoint = FALSE;
}
*/

// Step by step execution
/*
static uint32 FxStepOver (uint32 nInstructions)
{
	uint32	vCount;

	fx_readRegisterSpace();

	if (!fx_checkStartAddress())
	{
		CF(G);
	#if 0
		GSU.vIllegalAddress = (GSU.vPrgBankReg << 24) | R15;
		return (FX_ERROR_ILLEGAL_ADDRESS);
	#else
		return (0);
	#endif
	}

	if (PIPE >= 0xf0)
		GSU.vStepPoint = USEX16(R15 + 3);
	else
	if ((PIPE >= 0x05 && PIPE <= 0x0f) || (PIPE >= 0xa0 && PIPE <= 0xaf))
		GSU.vStepPoint = USEX16(R15 + 2);
	else
		GSU.vStepPoint = USEX16(R15 + 1);

	vCount = fx_step_over(nInstructions);

	fx_writeRegisterSpace();

	if (GSU.vErrorCode)
		return (GSU.vErrorCode);
	else
		return (vCount);
}
*/

// Errors
/*
static int FxGetErrorCode (void)
{
	return (GSU.vErrorCode);
}
*/

/*
static int FxGetIllegalAddress (void)
{
	return (GSU.vIllegalAddress);
}
*/

// Access to internal registers
/*
static uint32 FxGetColorRegister (void)
{
	return (GSU.vColorReg & 0xff);
}
*/

/*
static uint32 FxGetPlotOptionRegister (void)
{
	return (GSU.vPlotOptionReg & 0x1f);
}
*/

/*
static uint32 FxGetSourceRegisterIndex (void)
{
	return (GSU.pvSreg - GSU.avReg);
}
*/

/*
static uint32 FxGetDestinationRegisterIndex (void)
{
	return (GSU.pvDreg - GSU.avReg);
}
*/

// Get the byte currently in the pipe
/*
static uint8 FxPipe (void)
{
	return (GSU.vPipe);
}
*/
