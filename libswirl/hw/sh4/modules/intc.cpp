/*
	SH4/mod/intc

	Implements the register interface of the sh4 interrupt controller.
	For the actual implementation of interrupt caching/handling logic, look at sh4_interrupts.cpp

	--
*/

#include "types.h"
#include "../sh4_interrupts.h"
#include "../sh4_mmr.h"


//Register writes need interrupt re-testing !

void write_INTC_IPRA(void* that, u32 addr, u32 data)
{
	if (INTC_IPRA.reg_data!=(u16)data)
	{
		INTC_IPRA.reg_data=(u16)data;
		SIIDRebuild();	//we need to rebuild the table
	}
}
void write_INTC_IPRB(void* that, u32 addr, u32 data)
{
	if (INTC_IPRB.reg_data!=(u16)data)
	{
		INTC_IPRB.reg_data=(u16)data;
		SIIDRebuild(); //we need to rebuild the table
	}
}
void write_INTC_IPRC(void* that, u32 addr, u32 data)
{
	if (INTC_IPRC.reg_data!=(u16)data)
	{
		INTC_IPRC.reg_data=(u16)data;
		SIIDRebuild(); //we need to rebuild the table
	}
}
static u32 read_INTC_IPRD(void* that, u32 addr)
{
	return 0;
}

//Init/Res/Term
void intc_init()
{
	//INTC ICR 0xFFD00000 0x1FD00000 16 0x0000 0x0000 Held Held Pclk
	sh4_rio_reg(sh4_cpu, INTC,INTC_ICR_addr,RIO_DATA,16);

	//INTC IPRA 0xFFD00004 0x1FD00004 16 0x0000 0x0000 Held Held Pclk
	sh4_rio_reg(sh4_cpu, INTC,INTC_IPRA_addr,RIO_WF,16,0,&write_INTC_IPRA);

	//INTC IPRB 0xFFD00008 0x1FD00008 16 0x0000 0x0000 Held Held Pclk
	sh4_rio_reg(sh4_cpu, INTC,INTC_IPRB_addr,RIO_WF,16,0,&write_INTC_IPRB);

	//INTC IPRC 0xFFD0000C 0x1FD0000C 16 0x0000 0x0000 Held Held Pclk
	sh4_rio_reg(sh4_cpu, INTC,INTC_IPRC_addr,RIO_WF,16,0,&write_INTC_IPRC);

	//INTC IPRD 0xFFD00010 0x1FD00010 16 0xDA74 0xDA74 Held Held Pclk	(SH7750S, SH7750R only)
	sh4_rio_reg(sh4_cpu, INTC,INTC_IPRD_addr,RIO_RO_FUNC,16,&read_INTC_IPRD);

	interrupts_init();
}

void intc_reset()
{
	INTC_ICR.reg_data = 0x0;
	INTC_IPRA.reg_data = 0x0;
	INTC_IPRB.reg_data = 0x0;
	INTC_IPRC.reg_data = 0x0;

	SIIDRebuild(); //rebuild the interrupts table

	interrupts_reset();
}

void intc_term()
{
	interrupts_term();
}

