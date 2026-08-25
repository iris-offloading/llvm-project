typedef unsigned v2u32 __attribute__((vector_size(8)));
unsigned long g8, g16;
int main(void) {
  v2u32 v = {0, 3};
  v[0] = __builtin_parity(g16 & 3);
  unsigned long b = __builtin_bit_cast(unsigned long, v);
  g8 = -b;                 /* keep b live as a 64-bit value */
  v[(unsigned char)b] = 0; /* index is 0 */
  g16 = v[0];
  return 0;
}
