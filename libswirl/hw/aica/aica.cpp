#include "aica.h"
#include "sgc_if.h"
#include "aica_mmio.h"
#include "aica_mem.h"
#include <math.h>
#include "hw/holly/holly_intc.h"
#include "hw/holly/sb.h"

#define SH4_IRQ_BIT (1<<(holly_SPU_IRQ&255))

CommonData_struct* CommonData;
DSPData_struct* DSPData;
InterruptInfo* MCIEB;
InterruptInfo* MCIPD;
InterruptInfo* MCIRE;
InterruptInfo* SCIEB;
InterruptInfo* SCIPD;
InterruptInfo* SCIRE;

//Interrupts
//arm side
u32 GetL(u32 witch)
{
	if (witch>7)
		witch=7; //higher bits share bit 7

	u32 bit=1<<witch;
	u32 rv=0;

	if (CommonData->SCILV0 & bit)
		rv=1;

	if (CommonData->SCILV1 & bit)
		rv|=2;
	
	if (CommonData->SCILV2 & bit)
		rv|=4;

	return rv;
}
void update_arm_interrupts()
{
	u32 p_ints=SCIEB->full & SCIPD->full;

	u32 Lval=0;
	if (p_ints)
	{
		u32 bit_value=1;//first bit
		//scan all interrupts , lo to hi bit.I assume low bit ints have higher priority over others
		for (u32 i=0;i<11;i++)
		{
			if (p_ints & bit_value)
			{
				//for the first one , Set the L reg & exit
				Lval=GetL(i);
				break;
			}
			bit_value<<=1; //next bit
		}
	}

	libARM_InterruptChange(p_ints,Lval);
}

//sh4 side
void UpdateSh4Ints()
{
	u32 p_ints = MCIEB->full & MCIPD->full;
	if (p_ints)
	{
		if ((SB_ISTEXT & SH4_IRQ_BIT )==0)
		{
			//if no interrupt is already pending then raise one :)
			asic_RaiseInterrupt(holly_SPU_IRQ);
		}
	}
	else
	{
		if (SB_ISTEXT&SH4_IRQ_BIT)
		{
			asic_CancelInterrupt(holly_SPU_IRQ);
		}
	}

}


AicaTimer timers[3];

void libAICA_TimeStep()
{
	for (int i=0;i<3;i++)
		timers[i].StepTimer(1);

	SCIPD->SAMPLE_DONE=1;

	if (settings.aica.NoBatch)
		AICA_Sample();

	//Make sure sh4/arm interrupt system is up to date :)
	update_arm_interrupts();
	UpdateSh4Ints();	
}

//Memory i/o
template<u32 sz>
void WriteAicaReg(u32 reg,u32 data)
{
	switch (reg)
	{
	case SCIPD_addr:
		verify(sz!=1);
		if (data & (1<<5))
		{
			SCIPD->SCPU=1;
			update_arm_interrupts();
		}
		//Read only
		return;

	case SCIRE_addr:
		{
			verify(sz!=1);
			SCIPD->full&=~(data /*& SCIEB->full*/ );	//is the & SCIEB->full needed ? doesn't seem like it
			data=0;//Write only
			update_arm_interrupts();
		}
		break;

	case MCIPD_addr:
		if (data & (1<<5))
		{
			verify(sz!=1);
			MCIPD->SCPU=1;
			UpdateSh4Ints();
		}
		//Read only
		return;

	case MCIRE_addr:
		{
			verify(sz!=1);
			MCIPD->full&=~data;
			UpdateSh4Ints();
			//Write only
		}
		break;

	case TIMER_A:
		WriteMemArr(aica_reg,reg,data,sz);
		timers[0].RegisterWrite();
		break;

	case TIMER_B:
		WriteMemArr(aica_reg,reg,data,sz);
		timers[1].RegisterWrite();
		break;

	case TIMER_C:
		WriteMemArr(aica_reg,reg,data,sz);
		timers[2].RegisterWrite();
		break;

	default:
		WriteMemArr(aica_reg,reg,data,sz);
		break;
	}
}



template void WriteAicaReg<1>(u32 reg,u32 data);
template void WriteAicaReg<2>(u32 reg,u32 data);

struct AICA_impl : AICA {

	s32 Init()
	{
		aica_init_mem();
		aica_mmio_Init();

		verify(sizeof(*CommonData) == 0x508);
		verify(sizeof(*DSPData) == 0x15C8);

		CommonData = (CommonData_struct*)&aica_reg[0x2800];
		DSPData = (DSPData_struct*)&aica_reg[0x3000];
		//slave cpu (arm7)

		SCIEB = (InterruptInfo*)&aica_reg[0x289C];
		SCIPD = (InterruptInfo*)&aica_reg[0x289C + 4];
		SCIRE = (InterruptInfo*)&aica_reg[0x289C + 8];
		//Main cpu (sh4)
		MCIEB = (InterruptInfo*)&aica_reg[0x28B4];
		MCIPD = (InterruptInfo*)&aica_reg[0x28B4 + 4];
		MCIRE = (InterruptInfo*)&aica_reg[0x28B4 + 8];

		sgc_Init();
		for (int i = 0; i < 3; i++)
			timers[i].Init(aica_reg, i);

		return rv_ok;
	}

	void Reset(bool manual)
	{
		if (!manual)
			aica_init_mem();
		sgc_Init();
		aica_mmio_Reset(manual);
	}

	void Term()
	{
		sgc_Term();
	}

	//Mainloop
	void Update(u32 Samples)
	{
		AICA_Sample32();
	}

	//Aica reads (both sh4&arm)
	u32 ReadReg(u32 addr, u32 size) {
		return libAICA_ReadReg(addr, size);
	}

	void WriteReg(u32 addr, u32 data, u32 size) {
		libAICA_WriteReg(addr, data, size);
	}
};

AICA* AICA::Create() {
	return new AICA_impl();
}
