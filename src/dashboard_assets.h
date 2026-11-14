#pragma once

// PlatformIO embeds these text files in flash with a terminating NUL.
extern const char kDashboardHtml[] asm("_binary_web_dashboard_html_start");
extern const char kDashboardJs[] asm("_binary_web_dashboard_js_start");
extern const char kDashboardCss[] asm("_binary_web_dashboard_css_start");
