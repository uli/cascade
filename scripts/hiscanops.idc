#include <idc.idc>


static main() {

auto count;
auto pc;
auto off;
auto dref;
auto n;

count = 0;
pc = ScreenEA();

while(pc != BADADDR) {
	n = -1;
	if (GetOpType(pc, 1) == 10)
		n = 1;
	else if (GetOpType(pc, 0) == 10)
		n = 0;
	if (n != -1) {
		if (Byte(pc + 1) & 1)
			off = Word(pc + 2);
		else
			off = Byte(pc + 2);
		if (off < 0xc000) {
			dref = Dfirst(pc);
			while (dref != BADADDR) {
				if (dref > 0xffff) {
					Message("removing bogus dref to %x\n", dref);
					del_dref(pc, dref);
				}
				dref = Dnext(pc, dref);
			}
		}
		if (Byte(pc + 1) != 0x18 /* off > 0x100 */ && off < 0xc000) {
			add_dref(pc, off, 0);
			//OpAlt(pc, 1, "unk_seg001_" + ltoa(off, 16) + "[]");
			if (off < 0x400)
				OpOffEx(pc, n, REF_OFF16, off, 0, 0);
			else
				OpOffEx(pc, n, REF_LOW16, off, 0, 0);
			Jump(pc);
			Message("%x gotcha no. %d: %x\n", pc, count, off);
	  	count++;
	  }
	}
	pc = NextHead(pc, MAXADDR);
	//if (count > 3) break;
}
}
