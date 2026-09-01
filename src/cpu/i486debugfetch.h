/* LICENSE>>
Copyright 2020 Soji Yamakawa (CaptainYS, http://www.ysflight.com)

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

<< LICENSE */

#pragma once

inline void i486DXCommon::DebugFetchOperand8(Instruction &inst,MemoryAccess::ConstPointer &ptr,const SegmentRegister &seg,unsigned int offset,const Memory &mem) const
{
	inst.operand[inst.operandLen++]=DebugFetchByte(inst.codeAddressSize,seg,offset,mem);
	++inst.numBytes;
}
inline void i486DXCommon::DebugPeekOperand8(unsigned int &operand,const Instruction &inst,const MemoryAccess::ConstPointer &ptr,const SegmentRegister &seg,unsigned int offset,const Memory &mem) const
{
	operand=DebugFetchByte(inst.codeAddressSize,seg,offset,mem);
}
inline void i486DXCommon::DebugFetchOperand16(Instruction &inst,MemoryAccess::ConstPointer &ptr,const SegmentRegister &seg,unsigned int offset,const Memory &mem) const
{
	inst.operand[inst.operandLen  ]=DebugFetchByte(inst.codeAddressSize,seg,offset  ,mem);
	inst.operand[inst.operandLen+1]=DebugFetchByte(inst.codeAddressSize,seg,offset+1,mem);
	offset+=2;
	inst.operandLen+=2;
	inst.numBytes+=2;
}
inline void i486DXCommon::DebugFetchOperand32(Instruction &inst,MemoryAccess::ConstPointer &ptr,const SegmentRegister &seg,unsigned int offset,const Memory &mem) const
{
	inst.operand[inst.operandLen  ]=DebugFetchByte(inst.codeAddressSize,seg,offset  ,mem);
	inst.operand[inst.operandLen+1]=DebugFetchByte(inst.codeAddressSize,seg,offset+1,mem);
	inst.operand[inst.operandLen+2]=DebugFetchByte(inst.codeAddressSize,seg,offset+2,mem);
	inst.operand[inst.operandLen+3]=DebugFetchByte(inst.codeAddressSize,seg,offset+3,mem);
	offset+=4;
	inst.operandLen+=4;
	inst.numBytes+=4;
}

inline unsigned int i486DXCommon::DebugFetchOperand16or32(Instruction &inst,MemoryAccess::ConstPointer &ptr,const SegmentRegister &seg,unsigned int offset,const Memory &mem) const
{
	if(16==inst.operandSize)
	{
		DebugFetchOperand16(inst,ptr,seg,offset,mem);
		return 2;
	}
	else // if(32==inst.operandSize)
	{
		DebugFetchOperand32(inst,ptr,seg,offset,mem);
		return 4;
	}
}

inline void i486DXCommon::DebugFetchImm8(Instruction &inst,MemoryAccess::ConstPointer &ptr,const SegmentRegister &seg,unsigned int offset,const Memory &mem) const
{
	inst.imm[0]=DebugFetchByte(inst.codeAddressSize,seg,offset,mem);
	++inst.numBytes;
}
inline void i486DXCommon::DebugFetchImm16(Instruction &inst,MemoryAccess::ConstPointer &ptr,const SegmentRegister &seg,unsigned int offset,const Memory &mem) const
{
	inst.imm[0]=DebugFetchByte(inst.codeAddressSize,seg,offset  ,mem);
	inst.imm[1]=DebugFetchByte(inst.codeAddressSize,seg,offset+1,mem);
	inst.numBytes+=2;
}
inline void i486DXCommon::DebugFetchImm32(Instruction &inst,MemoryAccess::ConstPointer &ptr,const SegmentRegister &seg,unsigned int offset,const Memory &mem) const
{
	inst.imm[0]=DebugFetchByte(inst.codeAddressSize,seg,offset  ,mem);
	inst.imm[1]=DebugFetchByte(inst.codeAddressSize,seg,offset+1,mem);
	inst.imm[2]=DebugFetchByte(inst.codeAddressSize,seg,offset+2,mem);
	inst.imm[3]=DebugFetchByte(inst.codeAddressSize,seg,offset+3,mem);
	inst.numBytes+=4;
}
inline unsigned int i486DXCommon::DebugFetchImm16or32(Instruction &inst,MemoryAccess::ConstPointer &ptr,const SegmentRegister &seg,unsigned int offset,const Memory &mem) const
{
	if(16==inst.operandSize)
	{
		DebugFetchImm16(inst,ptr,seg,offset,mem);
		return 2;
	}
	else // if(32==inst.operandSize)
	{
		DebugFetchImm32(inst,ptr,seg,offset,mem);
		return 4;
	}
}
