Description: Thao tác file an toàn trong workspace của task.
Keywords: file, tệp, đọc, ghi, lưu, thư mục

# File Operations

1. Luôn dùng đường dẫn tương đối bên trong workspace, ví dụ result.txt.
2. Dùng file_write với path và content; kiểm tra kết quả trả về là OK.
3. Dùng file_read với path; không đoán nội dung trước khi đọc.
4. Khi cần ghi đè, gọi file_write lại đúng đường dẫn.
5. Không dùng .. hoặc đường dẫn tuyệt đối để vượt workspace.
6. Sau chuỗi write/read, đối chiếu nội dung đọc lại với yêu cầu.
