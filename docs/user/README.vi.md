[English](../../README.md) | [Português (Brasil)](README.pt-BR.md) | [Español](README.es.md) | [Català](README.ca.md) | [Italiano](README.it.md) | [简体中文](README.zh-CN.md) | [Русский](README.ru.md) | [Bahasa Indonesia](README.id.md) | [Polski](README.pl.md) | **Tiếng Việt**

# Giới thiệu

Công cụ tự động port tệp thực thi sang Linux và Windows.

Bản build chỉ gồm relinker chạy được trên macOS, bao gồm cả Apple silicon.

[**Trình chuyển đổi web**](https://boykopovar.github.io/AnyPS5/) - các tệp được xử lý ngay trên thiết bị của bạn.

![anyps5_collage1](https://github.com/user-attachments/assets/ed2f0b2d-33d3-46d7-ab88-767ca872fc99)

Bao gồm một [relinker](../../core/relinker) chuyển đổi tệp thực thi sang định dạng gốc của hệ thống đích và các bản triển khai của [thư viện prx hệ thống](../../core/libs/prx) phù hợp cho liên kết động (dynamic linking). Không dùng giả lập hay tiến trình runtime riêng.

[**Discord**](https://discord.gg/x9naWVzC6A), [Hướng dẫn sử dụng](USAGE.md), [Hướng dẫn build](../dev/BUILD.md), [Kiến trúc](../dev/ARCHITECTURE.md), [Nợ kỹ thuật của dự án](../dev/TechnicalDebt.md), [quy ước phong cách code](../dev/CONVENTIONS.md), [đóng góp](../../CONTRIBUTING.md)

## Trạng thái

[![libraries](https://boykopovar.github.io/AnyPS5/badge-libraries.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![shader forms decoded, counted from the code](https://boykopovar.github.io/AnyPS5/badge-shaders.svg)](https://boykopovar.github.io/AnyPS5/progress.html) [![implemented in 7 days](https://boykopovar.github.io/AnyPS5/badge-week.svg)](https://boykopovar.github.io/AnyPS5/history.html)

[![progress map](https://boykopovar.github.io/AnyPS5/progress.svg)](https://boykopovar.github.io/AnyPS5/progress.html)

[Lịch sử tiến độ](https://boykopovar.github.io/AnyPS5/history.html)

<sub>* Thư viện hệ thống: tỷ lệ phần trăm các hàm mà dự án đã biết cho đến nay (được khai báo trong [core/libs/prx](../../core/libs/prx)), không phải toàn bộ mọi hàm hệ thống của PS5. Tổng số này tăng lên khi có thêm nhiều hàm được khai báo.</sub>

Các trạng thái không được hỗ trợ hoặc ngoài dự kiến sẽ ném ra một ngoại lệ C++, thường là `std::runtime_error`. Ngoại lệ không được bắt sẽ in `what()` của nó ra stderr và kết thúc tiến trình.

[Bộ biên dịch lại shader](../../core/shader/recompiler/Recompiler.cpp) tạo ra mã SPIR-V hợp lệ (được kiểm tra qua [Spirv-Tools](../../3rdparty/SPIRV-Tools) khi build với `ANYPS5_ENABLE_SPIRV_TOOLS`).

## Tương thích

Trạng thái từng game và các báo cáo kiểm thử nằm trong mục thảo luận [Game bring-up](https://github.com/boykopovar/AnyPS5/discussions/categories/game-bring-up), mỗi tựa game một chủ đề.

## Ánh xạ điều khiển

Hỗ trợ tay cầm được ánh xạ qua SDL, bao gồm cả cần analog và cò (trigger). Có thể cấu hình bàn phím và chuột bằng tệp `anyps5-input.ini`. Xem [ánh xạ điều khiển](INPUT_MAPPING.md) để biết các thiết bị được hỗ trợ và định dạng cấu hình.

## Tuyên bố miễn trừ trách nhiệm

Dự án này hướng tới mục đích tương thích, nghiên cứu, bảo tồn và khả năng tương tác. Dự án không bao gồm, phân phối hay yêu cầu phần mềm có bản quyền, firmware, khóa mã hóa hoặc thư viện độc quyền. Người dùng tự chịu trách nhiệm đảm bảo mọi tệp nhị phân dùng cùng dự án này được thu thập và sử dụng tuân thủ luật pháp hiện hành cũng như điều khoản cấp phép tương ứng của chúng.

AnyPS5 là một dự án độc lập và không liên kết, không được ủy quyền hay chứng thực bởi Sony Interactive Entertainment. "PlayStation" và "PS5" là các nhãn hiệu của Sony Interactive Entertainment Inc.

## Giấy phép

Dự án này được cấp phép duy nhất theo GNU General Public License phiên bản 2.
