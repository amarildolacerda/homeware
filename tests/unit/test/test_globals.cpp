unsigned long s_fake_millis = 0;
unsigned long millis() { return s_fake_millis; }
void fake_millis_set(unsigned long t) { s_fake_millis = t; }
void fake_millis_advance(unsigned long delta) { s_fake_millis += delta; }
