#include <unity.h>
#include "mock_display.h"
#include "page_manager.h"
#include "test_helper.h"
#include <cstring>

static int s_page1_called = 0;
static int s_page2_called = 0;
static int s_footer_called = 0;

static void page1(DisplayInterface& d) {
    s_page1_called++;
    d.print("Page 1");
}
static void page2(DisplayInterface& d) {
    s_page2_called++;
    d.print("Page 2");
}
static void footer(DisplayInterface& d) {
    s_footer_called++;
    d.print("Footer");
}

void test_pagemanager_no_pages() {
    MockDisplay mock;
    PageManager mgr(&mock);
    TEST_ASSERT_EQUAL(-1, mgr.loop());
}

void test_pagemanager_add_page() {
    MockDisplay mock;
    PageManager mgr(&mock);
    TEST_ASSERT_TRUE(mgr.add_page(page1));
    TEST_ASSERT_TRUE(mgr.add_page(page2));
    TEST_ASSERT_FALSE(mgr.add_page(nullptr));
}

void test_pagemanager_switches_page() {
    MockDisplay mock;
    PageManager mgr(&mock);
    mgr.add_page(page1);
    mgr.add_page(page2);
    mgr.set_page_interval(1);

    int first = mgr.loop();
    fake_millis_advance(10);
    int second = mgr.loop();
    TEST_ASSERT_NOT_EQUAL(first, second);
}

void test_pagemanager_footer() {
    MockDisplay mock;
    PageManager mgr(&mock);
    mgr.add_page(page1);
    mgr.add_page(page2);
    mgr.set_page_interval(1);
    mgr.set_footer(footer);

    fake_millis_advance(10);
    mgr.loop();
    TEST_ASSERT_TRUE(s_footer_called > 0);
}

void test_pagemanager_page_render() {
    MockDisplay mock;
    PageManager mgr(&mock);
    mgr.add_page(page1);
    mgr.add_page(page2);
    mgr.set_page_interval(1);

    fake_millis_advance(10);
    int page = mgr.loop();
    TEST_ASSERT_EQUAL(1, page);
    TEST_ASSERT_EQUAL(0, strcmp(mock.m_last_text, "Page 2"));
}

void test_pagemanager_cycles_back() {
    MockDisplay mock;
    PageManager mgr(&mock);
    mgr.add_page(page1);
    mgr.add_page(page2);
    mgr.set_page_interval(1);

    fake_millis_advance(10);
    mgr.loop(); // switches to 1
    fake_millis_advance(10);
    mgr.loop(); // switches to 0
    fake_millis_advance(10);
    mgr.loop(); // switches to 1
    fake_millis_advance(10);
    int page = mgr.loop(); // switches to 0, renders page 1
    TEST_ASSERT_EQUAL(0, page);
    TEST_ASSERT_EQUAL(0, strcmp(mock.m_last_text, "Page 1"));
}
