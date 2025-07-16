
#ifdef _MSC_VER
#ifdef _WIN64
extern "C" void VirtualFreeAndExitThread(void *, void *, void *);
#else
__declspec(naked) void __fastcall VirtualFreeAndExitThread(void *, void *,
                                                           void *) {
  __asm {
    push MEM_RELEASE
    push 0
    push ecx
    push dword ptr [esp + 0x10]
    jmp edx
    int 3
  }
}
#endif
#else
#ifdef _WIN64
__declspec(naked) void VirtualFreeAndExitThread(void *, void *, void *) {
  __asm__(
      "push %%r8\n"
      "mov %%rdx, %%rax\n"
      "mov $0x8000, %%r8\n"
      "mov $0, %%rdx\n"
      "jmp *%%rax\n"
      "int $3\n"
      :
      :
      : "rax", "rdx", "r8", "r9");
}
#else
__declspec(naked) void __fastcall VirtualFreeAndExitThread(void *, void *,
                                                           void *) {
  __asm__(
      "push $0x8000\n"
      "push $0\n"
      "push %%ecx\n"
      "push 0x10(%%esp)\n"
      "jmp *%%edx\n"
      "int $3\n"
      :
      :
      : "ecx", "edx");
}
#endif
#endif
