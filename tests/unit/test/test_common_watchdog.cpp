#include <unity.h>
#include "common_watchdog.h"
#include "test_helper.h"

static StableWatchdog wd;

// setUp/tearDown vem de test_native.cpp (fake_millis_set(0))

// Sem arm_from_start e nunca saudavel → nunca dispara
void test_wd_never_healthy_no_arm_never_fires(void) {
    wd.init(60000, 300000, false);
    fake_millis_advance(1000000);
    TEST_ASSERT_FALSE(wd.check(false, millis()));
}

// arm_from_start=true e nunca saudavel → dispara apos restart_ms
void test_wd_arm_from_start_fires_when_never_healthy(void) {
    fake_millis_advance(1000);
    wd.init(60000, 300000, true);
    fake_millis_advance(300001);
    TEST_ASSERT_TRUE(wd.check(false, millis()));
}

void test_wd_arm_from_start_not_fire_before_restart(void) {
    fake_millis_advance(1000);
    wd.init(60000, 300000, true);
    fake_millis_advance(299999);
    TEST_ASSERT_FALSE(wd.check(false, millis()));
}

// Saudavel continuamente → arma apos stable_reset_ms e nunca dispara
void test_wd_healthy_arms_and_never_fires(void) {
    wd.init(60000, 300000, false);
    for (int i = 0; i < 61; i++) { fake_millis_advance(1000); wd.check(true, millis()); }
    fake_millis_advance(1000000);
    TEST_ASSERT_FALSE(wd.check(true, millis()));
}

// Flip-flop: reconexoes breves (< stable_reset_ms) nao desarmam o watchdog
void test_wd_flipflop_does_not_disarm(void) {
    wd.init(60000, 300000, false);
    for (int i = 0; i < 61; i++) { fake_millis_advance(1000); wd.check(true, millis()); }
    for (int i = 0; i < 100; i++) {
        fake_millis_advance(30000); wd.check(false, millis()); // quebrado 30s
        fake_millis_advance(5000);  wd.check(true, millis());  // saudavel 5s
    }
    TEST_ASSERT_TRUE(wd.check(false, millis()));
}

// Saudavel por tempo suficiente, depois quebrado → dispara apos restart_ms
void test_wd_healthy_then_broken_fires(void) {
    wd.init(60000, 300000, false);
    for (int i = 0; i < 61; i++) { fake_millis_advance(1000); wd.check(true, millis()); }
    fake_millis_advance(300001);
    TEST_ASSERT_TRUE(wd.check(false, millis()));
}

// reset() desarma (sem arm_from_start) → nao dispara
void test_wd_reset_disarms(void) {
    wd.init(60000, 300000, false);
    for (int i = 0; i < 61; i++) { fake_millis_advance(1000); wd.check(true, millis()); }
    wd.reset();
    fake_millis_advance(300001);
    TEST_ASSERT_FALSE(wd.check(false, millis()));
}