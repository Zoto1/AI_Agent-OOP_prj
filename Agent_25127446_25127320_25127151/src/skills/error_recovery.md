# Kỹ năng Phục hồi Lỗi (Error Recovery)

Trong quá trình thực thi, nếu công cụ trả về lỗi, chạy không thành công, hoặc hệ thống cảnh báo bạn đang bị lặp thao tác:

1. Dừng ngay việc lặp lại chính xác hành động vừa gây ra lỗi.
2. Đọc kỹ thông báo lỗi được trả về.
3. Phân tích nguyên nhân: Do sai cú pháp gọi tool? Hay do file không tồn tại?
4. Đưa ra hướng giải quyết: Thử lại với tham số khác, hoặc chuyển sang dùng một công cụ khác phù hợp hơn. Nếu không thể tự khắc phục sau 2 lần thử, hãy dừng lại và báo lỗi chi tiết cho người dùng.