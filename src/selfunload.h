
#ifdef _MSC_VER
#ifdef _WIN64
extern "C" void VirtualFreeAndExitThread(void *, void *, void *);
#else
__declspec(naked) void __fastcall VirtualFreeAndExitThread(void *, void *,
                                                           void *) {
  __asm {
    push dword ptr [esp + 4]
    push MEM_RELEASE
    push 0
    push ecx
    jmp edx
    int 3
  }
}
#endif
#else
#ifdef _WIN64
__declspec(naked) void VirtualFreeAndExitThread(void *, void *, void *) {
  __asm__(
      "sub $0x20, %%rsp\n"
      "push %%r8\n"
      "mov $0x8000, %%r9\n"
      "mov $0, %%r8\n"
      "jmp *%%rdx\n"
      "int $3\n"
      :
      :
      : "r8", "r9", "rdx");
}
#else
__declspec(naked) void __fastcall VirtualFreeAndExitThread(void *, void *,
                                                           void *) {
  __asm__(
      "push 4(%%esp)\n"
      "push $0x8000\n"
      "push $0\n"
      "push %%ecx\n"
      "jmp *%%edx\n"
      "int $3\n"
      :
      :
      : "ecx", "edx");
}
#endif
#endif
