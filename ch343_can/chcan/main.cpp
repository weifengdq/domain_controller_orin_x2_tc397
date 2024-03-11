#include <linux/can.h>

#include <asio.hpp>
#include <chrono>
#include <cstdbool>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

int main(int argc, char *argv[]) {
  asio::io_context iocxt;

  // serial, 1 start bit, 8 data bits, 1 stop bit, no parity, 2.5M baud
  // CAN 100K, Serial 1M, 1 start bit, 8 data bits, 1 stop bit, no parity
  // CAN 250K, Serial 2M, 1 start bit, 6 data bits, 1 stop bit, no parity
  // CAN 500K, Serial 4M, 1 start bit, 6 data bits, 1 stop bit, no parity
  asio::serial_port ser(iocxt, "/dev/ttyACM0");
  ser.set_option(asio::serial_port::baud_rate(2000000));
  ser.set_option(asio::serial_port::character_size(6));
  ser.set_option(
      asio::serial_port::stop_bits(asio::serial_port::stop_bits::one));
  ser.set_option(asio::serial_port::parity(asio::serial_port::parity::none));
  ser.set_option(
      asio::serial_port::flow_control(asio::serial_port::flow_control::none));
  if (!ser.is_open()) {
    std::cerr << "Failed to open serial port" << std::endl;
    return -1;
  }

  constexpr uint8_t Bit0 = 0x00;
  constexpr uint8_t Bit1 = 0xFF;

  // can crc15 calculation
  auto crc15 = [&](const std::vector<uint8_t> &data) {
    bool crc[15] = {0};
    for (int i = 0; i < data.size(); i++) {
      bool inv = (data[i] == Bit1) ^ crc[14];
      crc[14] = crc[13] ^ inv;
      crc[13] = crc[12];
      crc[12] = crc[11];
      crc[11] = crc[10];
      crc[10] = crc[9] ^ inv;
      crc[9] = crc[8];
      crc[8] = crc[7] ^ inv;
      crc[7] = crc[6] ^ inv;
      crc[6] = crc[5];
      crc[5] = crc[4];
      crc[4] = crc[3] ^ inv;
      crc[3] = crc[2] ^ inv;
      crc[2] = crc[1];
      crc[1] = crc[0];
      crc[0] = inv;
    }
    uint16_t res = 0;
    for (int i = 0; i < 15; i++) {
      res |= crc[i] << i;
    }
    return res;
  };

  // fill stuff bits
  auto fsb_insert = [&](std::vector<uint8_t> &data) {
    uint8_t count = 0;
    bool last = true;
    std::vector<uint8_t> newdata;
    for (auto it = data.begin(); it != data.end(); it++) {
      bool current = *it == Bit0 ? false : true;
      newdata.push_back(*it);
      if (current == last) {
        count++;
      } else {
        count = 1;
      }
      if (count == 5) {
        newdata.push_back(current ? Bit0 : Bit1);
        count = 1;
        last = !current;
      } else {
        last = current;
      }
    }
    return newdata;
  };

  // socketcan frame to serial frame
  auto can2ser = [&](const can_frame &frame) {
    std::vector<uint8_t> data;
    uint32_t id = 0;
    bool is_extended = frame.can_id & CAN_EFF_FLAG ? true : false;
    bool is_remote = frame.can_id & CAN_RTR_FLAG;
    uint8_t dlc = frame.len;
    data.push_back(Bit0);  // SOF, Start of frame
    if (is_extended) {
      id = frame.can_id & CAN_EFF_MASK;
      // High 11 bits id
      for (uint8_t i = 0; i < 11; i++) {
        uint8_t tmp = ((uint8_t)(id >> (28 - i)) & 0x01) ? Bit1 : Bit0;
        data.push_back(tmp);
      }
      data.push_back(Bit1);  // SRR, Substitute remote request
      data.push_back(Bit1);  // IDE, Identifier extension
      // Low 18 bits id
      for (uint8_t i = 0; i < 18; i++) {
        uint8_t tmp = ((uint8_t)(id >> (17 - i)) & 0x01) ? Bit1 : Bit0;
        data.push_back(tmp);
      }
      // RTR
      if (is_remote) {
        data.push_back(Bit1);
      } else {
        data.push_back(Bit0);
      }
      data.push_back(Bit0);  // RB1, reserved bit 1
    } else {
      id = frame.can_id & CAN_SFF_MASK;
      for (uint8_t i = 0; i < 11; i++) {
        uint8_t tmp = ((uint8_t)(id >> (10 - i)) & 0x01) ? Bit1 : Bit0;
        data.push_back(tmp);
      }
      // RTR
      if (is_remote) {
        data.push_back(Bit1);
      } else {
        data.push_back(Bit0);
      }
      data.push_back(Bit0);  // IDE, Identifier extension
    }
    data.push_back(Bit0);  // RB0, reserved bit 0
    // 4 bits DLC
    for (uint8_t i = 0; i < 4; i++) {
      uint8_t tmp = ((uint8_t)(dlc >> (3 - i)) & 0x01) ? Bit1 : Bit0;
      data.push_back(tmp);
    }
    // 0~64 bits data
    for (uint8_t i = 0; i < frame.len; i++) {
      for (uint8_t j = 0; j < 8; j++) {
        uint8_t tmp = ((frame.data[i] >> (7 - j)) & 0x01) ? Bit1 : Bit0;
        data.push_back(tmp);
      }
    }
    // CRC15
    uint16_t crc = crc15(data);
    for (uint8_t i = 0; i < 15; i++) {
      uint8_t tmp = ((crc >> (14 - i)) & 0x01) ? Bit1 : Bit0;
      data.push_back(tmp);
    }
    data.push_back(Bit1);  // CRC Delimiter
    // ACK, ACK Delimiter, EOF, IFS will not be cared
    return data;
  };

  // test can2ser
  auto test = [&]() {
    static uint8_t cnt = 0;
    can_frame frame;
    // use 0x92345678, 8 bytes of 0x3C to test fill stuff bits
    frame.can_id = 0x12345678 | CAN_EFF_FLAG;
    frame.len = 8;
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

  iocxt.run();

  return 0;
}