  // test can2ser
  auto test = [&]() {
    static uint8_t cnt = 0;
    can_frame frame;
    // use 0x92345678, 8 bytes of 0x3C to test fill stuff bits
    frame.can_id = 0x12345678 | CAN_EFF_FLAG;
    frame.can_dlc = 8;
    frame.__pad = 0;
    frame.__res0 = 0;
    frame.len8_dlc = 0;
    for (uint8_t i = 0; i < 8; i++) {
      frame.data[i] = cnt + i;
    }
    cnt++;
    auto data0 = can2ser(frame);
    auto data = fsb_insert(data0);
    // asio::write(ser, asio::buffer(data));
    asio::async_write(
        ser, asio::buffer(data),
        [&](const asio::error_code &ec, std::size_t bytes_transferred) {
          if (ec) {
            std::cerr << "Write error: " << ec.message() << std::endl;
          }
        });
  };

  // timer
  auto ms = std::chrono::milliseconds(10);
  asio::steady_timer t(iocxt, ms);
  std::function<void(const asio::error_code &)> timer =
      [&](const asio::error_code &ec) {
        if (ec) {
          std::cerr << "Timer error: " << ec.message() << std::endl;
        } else {
          test();
        }
        t.expires_at(t.expiry() + ms);
        t.async_wait(timer);
      };
  t.async_wait(timer);


    uint8_t can_buffer[1024];
  std::function<void(const asio::error_code &)> can2ser_read =
      [&](const asio::error_code &ec, std::size_t bytes_transferred) {
        if (ec) {
          std::cerr << "Read error: " << ec.message() << std::endl;
        } else {
          can_frame frame;
          std::vector<uint8_t> data;
          for (int i = 0; i < bytes_transferred; i++) {
            data.push_back(can_buffer[i]);
          }
          auto data0 = fsb_insert(data);
          auto data = can2ser(frame);
          asio::async_write(
              ser, asio::buffer(data),
              [&](const asio::error_code &ec, std::size_t bytes_transferred) {
                if (ec) {
                  std::cerr << "Write error: " << ec.message() << std::endl;
                }
              });
        }
        can.async_read_some(asio::buffer(can_buffer), can2ser_read);
      };

  can.async_read_some(asio::buffer(can_buffer), can2ser_read);