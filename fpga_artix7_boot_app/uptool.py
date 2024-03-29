import asyncio
import serial_asyncio
import sys
from enum import Enum
import zlib
import os
from math import ceil
import argparse
import time

SECTOR_SIZE = 65536
PAGE_SIZE = 256
APP_BIN_OFFSET = 0x10000
APP_INFO_SIZE = 16
APP_INFO_OFFSET = -SECTOR_SIZE
APP_ISR_INFO_SIZE = 16
APP_ISR_SIZE = 0x50

objcopy = r'C:\Xilinx\Vitis\2023.2\gnu\microblaze\nt\bin\mb-objcopy.exe'
elf = r'C:\z\ws_vivado\fpga_boot_app\bs_vitis_embedded\app\build\app.elf'


async def elf2bin():
    print(f'convert {elf} to app.bin')
    os.system(f'{objcopy} -O binary {elf} app.bin')


class BootStatus(Enum):
    BOOT_UNKNOWN = 0
    BOOT_CHECK = 1
    BOOT_READY = 2
    BOOT_SAVE_BRICK = 3
    BOOT_RESET = 4
    BOOT_ENTER_BOOT = 5
    BOOT_ENTER_APP = 6
    BOOT_NEXT_SET = 7
    BOOT_ERASE = 8
    BOOT_WRITE = 9
    BOOT_READ = 10
    BOOT_JUMP = 11
    BOOT_INFO = 12
    BOOT_STATUS_NUM = 13


def send_cmd(writer, cmd, addr, size):
    data = b''
    data += cmd.to_bytes(4, 'little')
    data += addr.to_bytes(4, 'little')
    data += size.to_bytes(4, 'little')
    crc = zlib.crc32(data).to_bytes(4, 'little')
    data = crc + data
    writer.write(data)
    # await writer.drain()


def ack_check(data, cmd, addr, size):
    crc32 = zlib.crc32(data[4:16])
    if crc32.to_bytes(4, 'little') != data[:4]:
        print(f'\033[31mcrc32 error\033[0m')
        return False
    if (0xFFFFFFFF - cmd) != int.from_bytes(data[4:8], 'little'):
        print(f'\033[31mcmd error\033[0m')
        return False
    if addr != int.from_bytes(data[8:12], 'little'):
        print(f'\033[31maddr error\033[0m')
        return False
    if size != int.from_bytes(data[12:16], 'little'):
        print(f'\033[31msize error\033[0m')
        return False
    return True


def sys_exit(running, task_read, task_write, writer):
    running = False
    time.sleep(0.2)
    # task_read.cancel()
    # task_write.cancel()
    asyncio.get_running_loop().stop()
    writer.close()
    sys.exit(0)


def parse_args():
    parser = argparse.ArgumentParser(description='FPGA serial boot utility')
    parser.add_argument(
        '--input', '-i', help='input file name')
    parser.add_argument(
        '--output', '-o', help='output file name')
    parser.add_argument('--serial', '-s', help='serial port')
    parser.add_argument('--baud', '-b', default=115200, help='baud rate')
    parser.add_argument('--addr', '-a', default='0',
                        help='address to read/write/erase')
    parser.add_argument('--size', '-z', default='0',
                        help='size to read/write/erase')
    parser.add_argument('--version', '-v', default='0.0.1',
                        help='version of app')
    parser.add_argument(
        '--cmd', '-c', help='cmd listen/save_brick/reset/enter_boot/enter_app/next/erase/write/write_only/read/check/jump/info/elf2bin/update')
    return parser.parse_args()


def data_print(data):
    for i in range(len(data)):
        print(f'\033[33m{data[i]:02X}\033[0m', end=' ')
        if i % 16 == 15:
            print('  ', end='')
            for j in range(i-15, i+1):
                if 32 < data[j] <= 126:
                    print(f'\033[32m{chr(data[j])}\033[0m', end='')
                else:
                    print('.', end='')
            print()
    if len(data) % 16 != 0:
        remain = 16 - len(data) % 16
        for i in range(remain):
            print('   ', end='')
        print('  ', end='')
        for j in range(len(data) // 16 * 16, len(data)):
            if 32 < data[j] <= 126:
                print(f'\033[32m{chr(data[j])}\033[0m', end='')
            else:
                print('.', end='')
        print()
    print(
        f'\033[35m■ {time.strftime("%X", time.localtime())}.{time.time_ns()//1000%1000000:06d}\033[0m')


async def read_from_serial(reader, running, r_queue):
    while running:
        data = await reader.read(10000)
        data_print(data)
        await r_queue.put(data)


async def write_to_serial(writer, running, w_queue):
    while running:
        data = await w_queue.get()
        writer.write(data)
        # await writer.drain()


def parse_addr_size(args):
    if args.addr == '':
        addr = 0
    elif args.addr.startswith('0x') or args.addr.startswith('0X'):
        addr = int(args.addr, 16)
    else:
        addr = int(args.addr)
    if args.size == '':
        size = 0
    elif args.size.startswith('0x') or args.size.startswith('0X'):
        size = int(args.size, 16)
    else:
        size = int(args.size)
    return addr, size


async def erase_func(args, r_queue, writer, addr, size):
    loop_cnt = 1
    if args.input:
        filesize = os.path.getsize(args.input)
        app_size = filesize - APP_BIN_OFFSET
        sectors = ceil(app_size / SECTOR_SIZE) + 1
        loop_cnt = sectors
        addr = addr - SECTOR_SIZE
        size = sectors * SECTOR_SIZE
        print(
            f'app size {app_size}, isr size {APP_ISR_SIZE}, erase size {size}, sectors {sectors}')
    while r_queue.qsize() > 0:
        await r_queue.get()
    last_size = r_queue.qsize()
    for i in range(loop_cnt):
        send_cmd(writer, BootStatus.BOOT_ERASE.value,
                 addr + i * SECTOR_SIZE, SECTOR_SIZE)
        while r_queue.qsize() == last_size:
            await asyncio.sleep(0.01)
        while r_queue.qsize() > 0:
            data = await r_queue.get()
            ack_check(data, BootStatus.BOOT_ERASE.value,
                      addr + i * SECTOR_SIZE, SECTOR_SIZE)
        last_size = r_queue.qsize()
        await asyncio.sleep(0.5)


async def next_func(r_queue, writer, size):
    if size != 272:
        size = 16
    while r_queue.qsize() > 0:
        await r_queue.get()
    send_cmd(writer, BootStatus.BOOT_NEXT_SET.value, 0, size)
    last_size = r_queue.qsize()
    while r_queue.qsize() == last_size:
        await asyncio.sleep(0.01)
    data = await r_queue.get()
    ack_check(data, BootStatus.BOOT_NEXT_SET.value, 0, size)


async def next_re_func(r_queue, writer, size):
    data = b''
    data += BootStatus.BOOT_NEXT_SET.value.to_bytes(4, 'little')
    data += 0x00000000.to_bytes(4, 'little')
    data += size.to_bytes(4, 'little')
    remain = 272 - size
    data += b'\0' * remain
    crc = zlib.crc32(data).to_bytes(4, 'little')
    data = crc + data
    while r_queue.qsize() > 0:
        await r_queue.get()
    writer.write(data)
    last_size = r_queue.qsize()
    while r_queue.qsize() == last_size:
        await asyncio.sleep(0.01)
    data = await r_queue.get()
    ack_check(data, BootStatus.BOOT_NEXT_SET.value, 0, size)


async def write_func(args, w_queue, r_queue):
    addr, size = parse_addr_size(args)
    filesize = os.path.getsize(args.input)
    loop_cnt = ceil((filesize - APP_BIN_OFFSET) / PAGE_SIZE)
    while r_queue.qsize() > 0:
        await r_queue.get()
    last_size = r_queue.qsize()
    version = int(args.version.split('.')[0]) << 24 | int(
        args.version.split('.')[1]) << 16 | int(args.version.split('.')[2])
    # write app
    app_data = b''
    isr_data = b''
    app_crc = 0
    app_addr = addr
    app_len = 0
    with open(args.input, 'rb') as file:
        data0 = file.read(APP_BIN_OFFSET)
        isr_data = data0[:APP_ISR_SIZE]
        for i in range(loop_cnt):
            size = 272
            data = BootStatus.BOOT_WRITE.value.to_bytes(
                4, 'little') + addr.to_bytes(4, 'little') + size.to_bytes(4, 'little')
            temp = file.read(PAGE_SIZE)
            app_data += temp
            app_len += len(temp)
            data += temp
            if i > 0 and i == loop_cnt - 1:
                remain = filesize - APP_BIN_OFFSET - i * PAGE_SIZE
                data += b'\0' * (PAGE_SIZE - remain)
                print(
                    f'last loop, remain {remain}, padding {PAGE_SIZE-remain} bytes')
            crc = zlib.crc32(data).to_bytes(4, 'little')
            data = crc + data
            await w_queue.put(data)
            print(f'write {i+1}/{loop_cnt}')
            while r_queue.qsize() == last_size:
                await asyncio.sleep(0.01)
            while r_queue.qsize() > 0:
                data = await r_queue.get()
                ack_check(data, BootStatus.BOOT_WRITE.value, addr, size)
            last_size = r_queue.qsize()
            addr += PAGE_SIZE
    # write app_info, app_isr_info and isr
    print(f'write app_info, app_isr_info and isr, addr {app_addr}')
    size = 272
    data = BootStatus.BOOT_WRITE.value.to_bytes(
        4, 'little') + (app_addr - SECTOR_SIZE).to_bytes(4, 'little') + size.to_bytes(4, 'little')
    # app_info
    app_info = b''
    app_info += version.to_bytes(4, 'little')
    app_info += app_addr.to_bytes(4, 'little')
    app_info += app_len.to_bytes(4, 'little')
    app_data = app_info + app_data
    app_crc = zlib.crc32(app_data)
    app_info = app_crc.to_bytes(4, 'little') + app_info
    data += app_info
    # app_isr_info
    app_isr_info = b''
    app_isr_info += version.to_bytes(4, 'little')
    app_isr_info += (app_addr - SECTOR_SIZE + 32).to_bytes(4, 'little')
    app_isr_info += APP_ISR_SIZE.to_bytes(4, 'little')
    app_isr_data = app_isr_info + isr_data
    app_isr_crc = zlib.crc32(app_isr_data)
    app_isr_info = app_isr_crc.to_bytes(4, 'little') + app_isr_info
    data += app_isr_info
    # isr
    data += isr_data
    data += b'\0' * (PAGE_SIZE - APP_ISR_SIZE - 32)
    data_crc = zlib.crc32(data).to_bytes(4, 'little')
    data = data_crc + data
    await w_queue.put(data)
    while r_queue.qsize() == last_size:
        await asyncio.sleep(0.01)
    while r_queue.qsize() > 0:
        data = await r_queue.get()
        if r_queue.qsize() != 0:
            ack_check(data, BootStatus.BOOT_WRITE.value, addr, size)
    # last_size = r_queue.qsize()
    print(f'write app_info, app_isr_info and isr end')


async def read_func(args, r_queue, writer, addr, size):
    send_cmd(writer, BootStatus.BOOT_READ.value, addr, size)
    timeout = 0.1
    last_size = r_queue.qsize()
    while timeout > 0:
        if r_queue.qsize() > last_size:
            timeout = 0.1
        last_size = r_queue.qsize()
        timeout -= 0.01
        await asyncio.sleep(0.01)
    data = b''
    while r_queue.qsize() > 0:
        data += await r_queue.get()
    data = data[16:]
    print(f'read data size: {len(data)}')
    if args.output:
        with open(args.output, 'wb') as file:
            file.write(data)


async def info_func(args, r_queue, writer):
    if args.input:
        print('input file info:')
        print(f'  file: {args.input}')
        size = os.path.getsize(args.input) - APP_BIN_OFFSET
        print(f'  size: {size}')
        print(f'  sectors: {ceil(size/SECTOR_SIZE)}')
        print(f'  pages: {ceil(size/PAGE_SIZE)}')
    else:
        send_cmd(writer, BootStatus.BOOT_INFO.value, 0, 0)
        data = await r_queue.get()
        ack_check(data, BootStatus.BOOT_INFO.value, 0, 0)
        print(f'✌️: {data[16:].decode()}', end='')
        # data contain boot or app or unknown
        current = data[16:].decode().split(' ')[1][:-1]
        # print(f'current: {current}')
        return current


async def check_func(args, r_queue, writer):
    send_cmd(writer, BootStatus.BOOT_CHECK.value, 0, 0)
    timeout = 1
    last_size = r_queue.qsize()
    while timeout > 0:
        if r_queue.qsize() > last_size:
            timeout = 1
        last_size = r_queue.qsize()
        timeout -= 0.01
        await asyncio.sleep(0.01)
    data = b''
    while r_queue.qsize() > 0:
        data += await r_queue.get()
    if len(data) <= 16:
        print(f'check timeout, no response')
        return False
    data = data[16:]
    print(f'read data size: {len(data)}\n{data.decode()}')
    # contain ok or ERROR
    if 'ok' in data.decode():
        return True
    else:
        return False
    # data = await r_queue.get()
    # ack_check(data, BootStatus.BOOT_CHECK.value, 0, 0)
    # print(f'✌️: {data[16:].decode()}')


async def jump_func(args, r_queue, writer):
    send_cmd(writer, BootStatus.BOOT_JUMP.value, 0, 0)
    timeout = 0.1
    last_size = r_queue.qsize()
    while timeout > 0:
        if r_queue.qsize() > last_size:
            timeout = 0.1
        last_size = r_queue.qsize()
        timeout -= 0.01
        await asyncio.sleep(0.01)
    data = b''
    while r_queue.qsize() > 0:
        data += await r_queue.get()
    data = data[16:]
    print(data.decode())


async def save_brick(args, r_queue, writer):
    send_cmd(writer, BootStatus.BOOT_SAVE_BRICK.value, 0, 0)
    timeout = 0.1
    last_size = r_queue.qsize()
    while timeout > 0:
        if r_queue.qsize() > last_size:
            timeout = 0.1
        last_size = r_queue.qsize()
        timeout -= 0.01
        await asyncio.sleep(0.01)
    data = b''
    while r_queue.qsize() > 0:
        data += await r_queue.get()
    return ack_check(data, BootStatus.BOOT_SAVE_BRICK.value, 0, 0)


async def enter_boot(args, r_queue, writer):
    current = await info_func(args, r_queue, writer)
    print("current    :", current)
    savebrick = False
    if current == 'app':
        await jump_func(args, r_queue, writer)
        timeout = 1
        while timeout > 0:
            savebrick = await save_brick(args, r_queue, writer)
            timeout -= 0.1
            await asyncio.sleep(0.01)
        current = await info_func(args, r_queue, writer)
        if current != 'boot':
            print('failed: can not jump to boot')


async def cmd_handler(running, r_queue, w_queue, args, task_read, task_write, writer):
    while running:
        if args.cmd == 'listen':
            while running:
                await asyncio.sleep(1)
        elif args.cmd == 'save_brick':
            pass
        elif args.cmd == 'reset':
            send_cmd(writer, BootStatus.BOOT_RESET.value, 0, 0)
            await asyncio.sleep(0.2)
            pass
        elif args.cmd == 'enter_boot':
            await enter_boot(args, r_queue, writer)
            pass
        elif args.cmd == 'enter_app':
            pass
        elif args.cmd == 'next':
            addr, size = parse_addr_size(args)
            await next_func(r_queue, writer, size)
        elif args.cmd == 'erase':
            addr, size = parse_addr_size(args)
            await erase_func(args, r_queue, writer, addr, size)
        elif args.cmd == 'write':
            addr, size = parse_addr_size(args)
            await erase_func(args, r_queue, writer, addr, size)
            await next_func(r_queue, writer, 272)
            await write_func(args, w_queue, r_queue)
            await next_re_func(r_queue, writer, 16)
            print('write done')
        elif args.cmd == 'write_only':
            await next_func(r_queue, writer, 272)
            await write_func(args, w_queue, r_queue)
            await next_re_func(r_queue, writer, 16)
        elif args.cmd == 'read':
            addr, size = parse_addr_size(args)
            await read_func(args, r_queue, writer, addr, size)
        elif args.cmd == 'check':
            await check_func(args, r_queue, writer)
        elif args.cmd == 'jump':
            await jump_func(args, r_queue, writer)
            pass
        elif args.cmd == 'info':
            await info_func(args, r_queue, writer)
        elif args.cmd == 'elf2bin':
            await elf2bin()
        elif args.cmd == 'update':
            await elf2bin()
            current = await info_func(args, r_queue, writer)
            args.addr = '0x00400000'
            print("current    :", current)
            # if current is boot
            savebrick = True
            if current == 'app':
                await jump_func(args, r_queue, writer)
                timeout = 1
                # time.sleep(0.1)
                # current = await info_func(args, r_queue, writer)
                while timeout > 0:
                    savebrick = await save_brick(args, r_queue, writer)
                    timeout -= 0.1
                    await asyncio.sleep(0.01)
                args.input = ''
                current = await info_func(args, r_queue, writer)
                if current != 'boot':
                    print('update failed: can not jump to boot')
            args.input = 'app.bin'
            if current == 'boot':
                if savebrick:
                    print('save brick ok')
                    await erase_func(args, r_queue, writer, 0x00400000, 0)
                    await next_func(r_queue, writer, 272)
                    await write_func(args, w_queue, r_queue)
                    await next_re_func(r_queue, writer, 16)
                    check_result = await check_func(args, r_queue, writer)
                    if check_result:
                        print('check ok, jump to app')
                        await jump_func(args, r_queue, writer)
                else:
                    print('save brick failed')
        else:
            print('unknown cmd:', args.cmd)
        running = False
        task_read.cancel()
        task_write.cancel()
        writer.close()
        return


async def main():
    args = parse_args()
    reader, writer = await serial_asyncio.open_serial_connection(url=args.serial, baudrate=args.baud)
    running = True
    w_queue = asyncio.Queue(8000)  # max 8000*128 bytes ? 2MB
    r_queue = asyncio.Queue(8000)
    task_read = asyncio.create_task(
        read_from_serial(reader, running, r_queue))
    task_write = asyncio.create_task(write_to_serial(writer, running, w_queue))
    task_cmd = asyncio.create_task(
        cmd_handler(running, r_queue, w_queue, args, task_read, task_write, writer))
    try:
        await task_read
        await task_write
        await task_cmd
    except asyncio.CancelledError or KeyboardInterrupt:
        sys_exit(running, task_read, task_write, writer)

if __name__ == '__main__':
    asyncio.run(main())
