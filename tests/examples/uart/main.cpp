static char *uart = (char *)0x10000000;

void print_ch(char ch) { *uart = ch; }

void print(const char *msg) {
  const char *ch = msg;
  while (*ch != '\0') {
    print_ch(*ch);
    ch++;
  }
}

int main() {
  print("test");
  return 0;
}
