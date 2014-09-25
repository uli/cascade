#include <idc.idc>
static main() {
auto target;
auto bank;
auto addr;
auto linaddr;

auto h;
auto sh;

auto count;

count = 0;
h = ScreenEA();

while (1) {
	h = NextHead(h, MAXADDR);
	if (h == BADADDR)
		break;
  sh = h - SegStart(h) + 0xc000;
  if (Byte(h) == 0xef) {
    target = (sh + Word(h + 1) + 3) & 0xffff;
    Message("target %x (h %x sh %x h+1 %x)\n", target, h, sh, Word(h+1));
  }
  else {
    //Message("no lcall\n");
    continue;
  }
	Jump(h);
  if (target < 0xc000) {
  	OpOffEx(h, 0, REF_OFF16, target, 0, 0);
  	AddCodeXref(h, target, fl_CN);
  }
  if (target == 0x21d3) {
    if (Byte(h - 3) == 0xb1 && Byte(h - 1) == 0x56) {
      bank = Byte(h - 2);
    }
    else {
      Message("no ldb dl\n");
      continue;
    }
    if (Byte(h-7) == 0xa1 && Byte(h - 4) == 0x54) {
      addr = Word(h - 6);
      linaddr = (bank + 3) * 0x4000 + addr - 0xc000;
      Message("Hooray! Far call to %x:%x (linear %x)\n", bank, addr, linaddr);
      AddCodeXref(h, linaddr, fl_CF);
      OpOffEx(h - 7, 1, REF_LOW16, linaddr, (bank + 3) * 0x4000 - 0xc000, 0);
    }
    else {
      Message("no ld cx\n");
      continue;
    }

    if (Byte(h - 2) == 0x35 && GetOperandValue(h - 7, 1) == 0xc353) {
      if (Byte(h - 11) == 0xa1) {
        OpOffEx(h - 11, 1, REF_LOW16, Word(h - 10), 0, 0);
	MakeComm(h - 11, "string buffer");
      }
      if (Byte(h - 14) == 0xc9) MakeComm(h - 14, "length");
      if (Byte(h - 17) == 0xc9) MakeComm(h - 17, "bank");
      if (Byte(h - 20) == 0xc9) {
        add_dref(h - 20, Word(h - 16) * 0x4000 + Word(h - 19), 0);
        OpOffEx(h - 20, 0, REF_LOW16, Word(h - 16) * 0x4000 + Word(h - 19), Word(h - 16) * 0x4000, 0);
      }
    }
  }
  else {
    Message("not a call to 0x21d3\n");
    continue;
  }
  count++;
  //if (count > 10) break;
}

}
