#pragma once

#include <tests/common/check.h>

#include <string>

namespace Kbi::Tests {

void testFft();
void testAnalyzer();
void testRollingMean();
void testDecimate();
void testPostprocess();
void testTimeline();
void testBase64();
void testLogStore();
void testChartInteraction();
void testUsbPollingRate();
void testPointSnapper();
void testParser(const std::string& path);
void testDatetime();
void testResultDevices();
void testLegacyParser(const std::string& path);

} // namespace Kbi::Tests
