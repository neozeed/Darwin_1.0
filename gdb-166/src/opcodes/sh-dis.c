/* Disassemble SH instructions.
   Copyright (C) 1993, 94, 95, 96, 97, 1998 Free Software Foundation, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include <stdio.h>
#define STATIC_TABLE
#define DEFINE_TABLE

#include "sh-opc.h"
#include "dis-asm.h"

#define LITTLE_BIT 2

static void
print_movxy (op, rn, rm, fprintf_fn, stream)
     sh_opcode_info *op;
     int rn, rm;
     fprintf_ftype fprintf_fn;
     void *stream;
{
  int n;

  fprintf_fn (stream,"%s\t", op->name);
  for (n = 0; n < 2; n++)
    {
      switch (op->arg[n])
	{
	case A_IND_N:
	  fprintf_fn (stream, "@r%d", rn);	
	  break;
	case A_INC_N:
	  fprintf_fn (stream, "@r%d+", rn);	
	  break;
	case A_PMOD_N:
	  fprintf_fn (stream, "@r%d+r8", rn);	
	  break;
	case A_PMODY_N:
	  fprintf_fn (stream, "@r%d+r9", rn);	
	  break;
	case DSP_REG_M:
	  fprintf_fn (stream, "a%c", '0' + rm);
	  break;
	case DSP_REG_X:
	  fprintf_fn (stream, "x%c", '0' + rm);
	  break;
	case DSP_REG_Y:
	  fprintf_fn (stream, "y%c", '0' + rm);
	  break;
	default:
	  abort ();
	}
      if (n == 0)
	fprintf_fn (stream, ",");	
    }
}

/* Print a double data transfer insn.  INSN is just the lower three
   nibbles of the insn, i.e. field a and the bit that indicates if
   a parallel processing insn follows.
   Return nonzero if a field b of a parallel processing insns follows.  */
static void
print_insn_ddt (insn, info)
     int insn;
     struct disassemble_info *info;
{
  fprintf_ftype fprintf_fn = info->fprintf_func;
  void *stream = info->stream;

  /* If this is just a nop, make sure to emit something.  */
  if (insn == 0x000)
    fprintf_fn (stream, "nopx\tnopy");

  /* If a parallel processing insn was printed before,
     and we got a non-nop, emit a tab.  */
  if ((insn & 0x800) && (insn & 0x3ff))
    fprintf_fn (stream, "\t");

  /* Check if either the x or y part is invalid.  */
  if (((insn & 0xc) == 0 && (insn & 0x2a0))
      || ((insn & 3) == 0 && (insn & 0x150)))
    fprintf_fn (stream, ".word 0x%x", insn);
  else
    {
      static sh_opcode_info *first_movx, *first_movy;
      sh_opcode_info *opx, *opy;
      int insn_x, insn_y;

      if (! first_movx)
	{
	  for (first_movx = sh_table; first_movx->nibbles[1] != MOVX; )
	    first_movx++;
	  for (first_movy = first_movx; first_movy->nibbles[1] != MOVY; )
	    first_movy++;
	}
      insn_x = (insn >> 2) & 0xb;
      if (insn_x)
	{
	  for (opx = first_movx; opx->nibbles[2] != insn_x; ) opx++;
	  print_movxy (opx, ((insn >> 9) & 1) + 4, (insn >> 7) & 1,
		       fprintf_fn, stream);
	}
      insn_y = (insn & 3) | ((insn >> 1) & 8);
      if (insn_y)
	{
	  if (insn_x)
	    fprintf_fn (stream, "\t");
	  for (opy = first_movy; opy->nibbles[2] != insn_y; ) opy++;
	  print_movxy (opy, ((insn >> 8) & 1) + 6, (insn >> 6) & 1,
		       fprintf_fn, stream);
	}
    }
}

static void
print_dsp_reg (rm, fprintf_fn, stream)
     int rm;
     fprintf_ftype fprintf_fn;
     void *stream;
{
  switch (rm)
    {
    case A_A1_NUM:
      fprintf_fn (stream, "a1");
      break;
    case A_A0_NUM:
      fprintf_fn (stream, "a0");
      break;
    case A_X0_NUM:
      fprintf_fn (stream, "x0");
      break;
    case A_X1_NUM:
      fprintf_fn (stream, "x1");
      break;
    case A_Y0_NUM:
      fprintf_fn (stream, "y0");
      break;
    case A_Y1_NUM:
      fprintf_fn (stream, "y1");
      break;
    case A_M0_NUM:
      fprintf_fn (stream, "m0");
      break;
    case A_A1G_NUM:
      fprintf_fn (stream, "a1g");
      break;
    case A_M1_NUM:
      fprintf_fn (stream, "m1");
      break;
    case A_A0G_NUM:
      fprintf_fn (stream, "a0g");
      break;
    default:
      fprintf_fn (stream, "0x%x", rm);
      break;
    }
}

static void
print_insn_ppi (field_b, info)
     int field_b;
     struct disassemble_info *info;
{
  static char *sx_tab[] = {"x0","x1","a0","a1"};
  static char *sy_tab[] = {"y0","y1","m0","m1"};
  fprintf_ftype fprintf_fn = info->fprintf_func;
  void *stream = info->stream;
  int nib1, nib2, nib3;
  char *dc;
  sh_opcode_info *op;

  if ((field_b & 0xe800) == 0)
    {
      fprintf_fn (stream, "psh%c\t#%d,",
		  field_b & 0x1000 ? 'a' : 'l',
		  (field_b >> 4) & 127);
      print_dsp_reg (field_b & 0xf, fprintf_fn, stream);
      return;
    }
  if ((field_b & 0xc000) == 0x4000 && (field_b & 0x3000) != 0x1000)
    {
      static char *du_tab[] = {"x0","y0","a0","a1"};
      static char *se_tab[] = {"x0","x1","y0","a1"};
      static char *sf_tab[] = {"y0","y1","x0","a1"};
      static char *sg_tab[] = {"m0","m1","a0","a1"};

      if (field_b & 0x2000)
	{
	  fprintf_fn (stream, "p%s %s,%s,%s\t",
		      (field_b & 0x1000) ? "add" : "sub",
		      sx_tab[(field_b >> 6) & 3],
		      sy_tab[(field_b >> 4) & 3],
		      du_tab[(field_b >> 0) & 3]);
	}
      fprintf_fn (stream, "pmuls%c%s,%s,%s",
		  field_b & 0x2000 ? ' ' : '\t',
		  se_tab[(field_b >> 10) & 3],
		  sf_tab[(field_b >>  8) & 3],
		  sg_tab[(field_b >>  2) & 3]);
      return;
    }

  nib1 = PPIC;
  nib2 = field_b >> 12 & 0xf;
  nib3 = field_b >> 8 & 0xf;
  switch (nib3 & 0x3)
    {
    case 0:
      dc = "";
      nib1 = PPI3;
      break;
    case 1:
      dc = "";
      break;
    case 2:
      dc = "dct ";
      nib3 -= 1;
      break;
    case 3:
      dc = "dcf ";
      nib3 -= 2;
      break;
    }
  for (op = sh_table; op->name; op++)
    {
      if (op->nibbles[1] == nib1
	  && op->nibbles[2] == nib2
	  && op->nibbles[3] == nib3)
	{
	  int n;

	  fprintf_fn (stream, "%s%s\t", dc, op->name);
	  for (n = 0; n < 3 && op->arg[n] != A_END; n++) 
	    {
	      if (n && op->arg[1] != A_END)
		fprintf_fn (stream, ",");
	      switch (op->arg[n]) 
		{
		case DSP_REG_N:
		  print_dsp_reg (field_b & 0xf, fprintf_fn, stream);
		  break;
		case DSP_REG_X:
		  fprintf_fn (stream, sx_tab[(field_b >> 6) & 3]);
		  break;
		case DSP_REG_Y:
		  fprintf_fn (stream, sy_tab[(field_b >> 4) & 3]);
		  break;
		case A_MACH:
		  fprintf_fn (stream, "mach");
		  break;
		case A_MACL:
		  fprintf_fn (stream ,"macl");
		  break;
		default:
		  abort ();
		}
	    }
	  return;
	}
    }
  /* Not found.  */
  fprintf_fn (stream, ".word 0x%x", field_b);
}

static int 
print_insn_shx (memaddr, info)
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  fprintf_ftype fprintf_fn = info->fprintf_func;
  void *stream = info->stream;
  unsigned char insn[2];
  unsigned char nibs[4];
  int status;
  bfd_vma relmask = ~ (bfd_vma) 0;
  sh_opcode_info *op;
  int target_arch;

  switch (info->mach)
    {
    case bfd_mach_sh:
      target_arch = arch_sh1;
      break;
    case bfd_mach_sh2:
      target_arch = arch_sh2;
      break;
    case bfd_mach_sh_dsp:
      target_arch = arch_sh_dsp;
      break;
    case bfd_mach_sh3:
      target_arch = arch_sh3;
      break;
    case bfd_mach_sh3_dsp:
      target_arch = arch_sh3_dsp;
      break;
    case bfd_mach_sh3e:
      target_arch = arch_sh3e;
      break;
    case bfd_mach_sh4:
      target_arch = arch_sh4;
      break;
    default:
      abort ();
    }

  status = info->read_memory_func (memaddr, insn, 2, info);

  if (status != 0) 
    {
      info->memory_error_func (status, memaddr, info);
      return -1;
    }

  if (info->flags & LITTLE_BIT) 
    {
      nibs[0] = (insn[1] >> 4) & 0xf;
      nibs[1] = insn[1] & 0xf;

      nibs[2] = (insn[0] >> 4) & 0xf;
      nibs[3] = insn[0] & 0xf;
    }
  else 
    {
      nibs[0] = (insn[0] >> 4) & 0xf;
      nibs[1] = insn[0] & 0xf;

      nibs[2] = (insn[1] >> 4) & 0xf;
      nibs[3] = insn[1] & 0xf;
    }

  if (nibs[0] == 0xf && (nibs[1] & 4) == 0 && target_arch & arch_sh_dsp_up)
    {
      if (nibs[1] & 8)
	{
	  int field_b;

	  status = info->read_memory_func (memaddr + 2, insn, 2, info);

	  if (status != 0) 
	    {
	      info->memory_error_func (status, memaddr + 2, info);
	      return -1;
	    }

	  if (info->flags & LITTLE_BIT) 
	    field_b = insn[1] << 8 | insn[0];
	  else
	    field_b = insn[0] << 8 | insn[1];

	  print_insn_ppi (field_b, info);
	  print_insn_ddt ((nibs[1] << 8) | (nibs[2] << 4) | nibs[3], info);
	  return 4;
	}
      print_insn_ddt ((nibs[1] << 8) | (nibs[2] << 4) | nibs[3], info);
      return 2;
    }
  for (op = sh_table; op->name; op++) 
    {
      int n;
      int imm = 0;
      int rn = 0;
      int rm = 0;
      int rb = 0;
      int disp_pc;
      bfd_vma disp_pc_addr = 0;

      if ((op->arch & target_arch) == 0)
	goto fail;
      for (n = 0; n < 4; n++)
	{
	  int i = op->nibbles[n];

	  if (i < 16) 
	    {
	      if (nibs[n] == i)
		continue;
	      goto fail;
	    }
	  switch (i)
	    {
	    case BRANCH_8:
	      imm = (nibs[2] << 4) | (nibs[3]);	  
	      if (imm & 0x80)
		imm |= ~0xff;
	      imm = ((char)imm) * 2 + 4 ;
	      goto ok;
	    case BRANCH_12:
	      imm = ((nibs[1]) << 8) | (nibs[2] << 4) | (nibs[3]);
	      if (imm & 0x800)
		imm |= ~0xfff;
	      imm = imm * 2 + 4;
	      goto ok;
	    case IMM_4:
	      imm = nibs[3];
	      goto ok;
	    case IMM_4BY2:
	      imm = nibs[3] <<1;
	      goto ok;
	    case IMM_4BY4:
	      imm = nibs[3] <<2;
	      goto ok;
	    case IMM_8:
	      imm = (nibs[2] << 4) | nibs[3];
	      goto ok;
	    case PCRELIMM_8BY2:
	      imm = ((nibs[2] << 4) | nibs[3]) <<1;
	      relmask = ~ (bfd_vma) 1;
	      goto ok;
	    case PCRELIMM_8BY4:
	      imm = ((nibs[2] << 4) | nibs[3]) <<2;
	      relmask = ~ (bfd_vma) 3;
	      goto ok;
	    case IMM_8BY2:
	      imm = ((nibs[2] << 4) | nibs[3]) <<1;
	      goto ok;
	    case IMM_8BY4:
	      imm = ((nibs[2] << 4) | nibs[3]) <<2;
	      goto ok;
	    case DISP_8:
	      imm = (nibs[2] << 4) | (nibs[3]);	  
	      goto ok;
	    case DISP_4:
	      imm = nibs[3];
	      goto ok;
	    case REG_N:
	      rn = nibs[n];
	      break;
	    case REG_M:
	      rm = nibs[n];
	      break;
	    case REG_NM:
	      rn = (nibs[n] & 0xc) >> 2;
	      rm = (nibs[n] & 0x3);
	      break;
	    case REG_B:
	      rb = nibs[n] & 0x07;
	      break;	
	    case SDT_REG_N:
	      /* sh-dsp: single data transfer.  */
	      rn = nibs[n];
	      if ((rn & 0xc) != 4)
		goto fail;
	      rn = rn & 0x3;
	      rn |= (rn & 2) << 1;
	      break;
	    case PPI:
	      goto fail;
	    default:
	      abort();
	    }
	}

    ok:
      fprintf_fn (stream,"%s\t", op->name);
      disp_pc = 0;
      for (n = 0; n < 3 && op->arg[n] != A_END; n++) 
	{
	  if (n && op->arg[1] != A_END)
	    fprintf_fn (stream, ",");
	  switch (op->arg[n]) 
	    {
	    case A_IMM:
	      fprintf_fn (stream, "#%d", (char)(imm));
	      break;
	    case A_R0:
	      fprintf_fn (stream, "r0");
	      break;
	    case A_REG_N:
	      fprintf_fn (stream, "r%d", rn);
	      break;
	    case A_INC_N:
	      fprintf_fn (stream, "@r%d+", rn);	
	      break;
	    case A_DEC_N:
	      fprintf_fn (stream, "@-r%d", rn);	
	      break;
	    case A_IND_N:
	      fprintf_fn (stream, "@r%d", rn);	
	      break;
	    case A_DISP_REG_N:
	      fprintf_fn (stream, "@(%d,r%d)", imm, rn);	
	      break;
	    case A_PMOD_N:
	      fprintf_fn (stream, "@r%d+r8", rn);	
	      break;
	    case A_REG_M:
	      fprintf_fn (stream, "r%d", rm);
	      break;
	    case A_INC_M:
	      fprintf_fn (stream, "@r%d+", rm);	
	      break;
	    case A_DEC_M:
	      fprintf_fn (stream, "@-r%d", rm);	
	      break;
	    case A_IND_M:
	      fprintf_fn (stream, "@r%d", rm);	
	      break;
	    case A_DISP_REG_M:
	      fprintf_fn (stream, "@(%d,r%d)", imm, rm);	
	      break;
	    case A_REG_B:
	      fprintf_fn (stream, "r%d_bank", rb);
	      break;
	    case A_DISP_PC:
	      disp_pc = 1;
	      disp_pc_addr = imm + 4 + (memaddr & relmask);
	      (*info->print_address_func) (disp_pc_addr, info);
	      break;
	    case A_IND_R0_REG_N:
	      fprintf_fn (stream, "@(r0,r%d)", rn);
	      break; 
	    case A_IND_R0_REG_M:
	      fprintf_fn (stream, "@(r0,r%d)", rm);
	      break; 
	    case A_DISP_GBR:
	      fprintf_fn (stream, "@(%d,gbr)",imm);
	      break;
	    case A_R0_GBR:
	      fprintf_fn (stream, "@(r0,gbr)");
	      break;
	    case A_BDISP12:
	    case A_BDISP8:
	      (*info->print_address_func) (imm + memaddr, info);
	      break;
	    case A_SR:
	      fprintf_fn (stream, "sr");
	      break;
	    case A_GBR:
	      fprintf_fn (stream, "gbr");
	      break;
	    case A_VBR:
	      fprintf_fn (stream, "vbr");
	      break;
	    case A_DSR:
	      fprintf_fn (stream, "dsr");
	      break;
	    case A_MOD:
	      fprintf_fn (stream, "mod");
	      break;
	    case A_RE:
	      fprintf_fn (stream, "re");
	      break;
	    case A_RS:
	      fprintf_fn (stream, "rs");
	      break;
	    case A_A0:
	      fprintf_fn (stream, "a0");
	      break;
	    case A_X0:
	      fprintf_fn (stream, "x0");
	      break;
	    case A_X1:
	      fprintf_fn (stream, "x1");
	      break;
	    case A_Y0:
	      fprintf_fn (stream, "y0");
	      break;
	    case A_Y1:
	      fprintf_fn (stream, "y1");
	      break;
	    case DSP_REG_M:
	      print_dsp_reg (rm, fprintf_fn, stream);
	      break;
	    case A_SSR:
	      fprintf_fn (stream, "ssr");
	      break;
	    case A_SPC:
	      fprintf_fn (stream, "spc");
	      break;
	    case A_MACH:
	      fprintf_fn (stream, "mach");
	      break;
	    case A_MACL:
	      fprintf_fn (stream ,"macl");
	      break;
	    case A_PR:
	      fprintf_fn (stream, "pr");
	      break;
	    case A_SGR:
	      fprintf_fn (stream, "sgr");
	      break;
	    case A_DBR:
	      fprintf_fn (stream, "dbr");
	      break;
	    case F_REG_N:
	      fprintf_fn (stream, "fr%d", rn);
	      break;
	    case F_REG_M:
	      fprintf_fn (stream, "fr%d", rm);
	      break;
	    case DX_REG_N:
	      if (rn & 1)
		{
		  fprintf_fn (stream, "xd%d", rn & ~1);
		  break;
		}
	    d_reg_n:
	    case D_REG_N:
	      fprintf_fn (stream, "dr%d", rn);
	      break;
	    case DX_REG_M:
	      if (rm & 1)
		{
		  fprintf_fn (stream, "xd%d", rm & ~1);
		  break;
		}
	    case D_REG_M:
	      fprintf_fn (stream, "dr%d", rm);
	      break;
	    case FPSCR_M:
	    case FPSCR_N:
	      fprintf_fn (stream, "fpscr");
	      break;
	    case FPUL_M:
	    case FPUL_N:
	      fprintf_fn (stream, "fpul");
	      break;
	    case F_FR0:
	      fprintf_fn (stream, "fr0");
	      break;
	    case V_REG_N:
	      fprintf_fn (stream, "fv%d", rn*4);
	      break;
	    case V_REG_M:
	      fprintf_fn (stream, "fv%d", rm*4);
	      break;
	    case XMTRX_M4:
	      fprintf_fn (stream, "xmtrx");
	      break;
	    default:
	      abort();
	    }
	}

#if 0
      /* This code prints instructions in delay slots on the same line
         as the instruction which needs the delay slots.  This can be
         confusing, since other disassembler don't work this way, and
         it means that the instructions are not all in a line.  So I
         disabled it.  Ian.  */
      if (!(info->flags & 1)
	  && (op->name[0] == 'j'
	      || (op->name[0] == 'b'
		  && (op->name[1] == 'r' 
		      || op->name[1] == 's'))
	      || (op->name[0] == 'r' && op->name[1] == 't')
	      || (op->name[0] == 'b' && op->name[2] == '.')))
	{
	  info->flags |= 1;
	  fprintf_fn (stream, "\t(slot ");
	  print_insn_shx (memaddr + 2, info);
	  info->flags &= ~1;
	  fprintf_fn (stream, ")");
	  return 4;
	}
#endif

      if (disp_pc && strcmp (op->name, "mova") != 0)
	{
	  int size;
	  bfd_byte bytes[4];

	  if (relmask == ~ (bfd_vma) 1)
	    size = 2;
	  else
	    size = 4;
	  status = info->read_memory_func (disp_pc_addr, bytes, size, info);
	  if (status == 0)
	    {
	      unsigned int val;

	      if (size == 2)
		{
		  if ((info->flags & LITTLE_BIT) != 0)
		    val = bfd_getl16 (bytes);
		  else
		    val = bfd_getb16 (bytes);
		}
	      else
		{
		  if ((info->flags & LITTLE_BIT) != 0)
		    val = bfd_getl32 (bytes);
		  else
		    val = bfd_getb32 (bytes);
		}
	      fprintf_fn (stream, "\t! 0x%x", val);
	    }
	}

      return 2;
    fail:
      ;

    }
  fprintf_fn (stream, ".word 0x%x%x%x%x", nibs[0], nibs[1], nibs[2], nibs[3]);
  return 2;
}

int 
print_insn_shl (memaddr, info)
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  int r;

  info->flags = LITTLE_BIT;
  r = print_insn_shx (memaddr, info);
  return r;
}

int 
print_insn_sh (memaddr, info)
     bfd_vma memaddr;
     struct disassemble_info *info;
{
  int r;

  info->flags = 0;
  r = print_insn_shx (memaddr, info);
  return r;
}
