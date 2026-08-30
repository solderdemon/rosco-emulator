// license:BSD-3-Clause
// copyright-holders:Ross Bamford, Xark
/*
 *------------------------------------------------------------
 *                            ___ ___ ___ ___
 *  ___ ___ ___ ___ ___      |  _| __|   |__ |
 * |  _| . |_ -|  _| . |     | . |__ | | | __|
 * |_| |___|___|___|___|_____|___|___|___|___|
 *                     |_____|
 * ------------------------------------------------------------
 * Copyright (c) 2022-2024 The rosco_6502 Open Source Project
 * MIT License
 * ------------------------------------------------------------
 *
 * rosco_6502 r4 single board computer.
 *
 * Hardware:
 * - WDC W65C02S @ 14MHz
 * - 16KB low RAM at $0000, 16 x 32KB banked RAM at $4000 (512KB total)
 * - 4 x 8KB banked ROM at $E000 (32KB total)
 * - XR68C681 DUART at $C000 (IRQ), two RS-232 channels, timers and the
 *   bit-banged SPI SD card, wired exactly as on the rosco_m68k
 * - Address decode and glue in two Atmel F22V10C PLDs
 *
 * The bank register lives at $0000. The address decoder ignores A0 when it
 * asserts BANKSEL, so $0001 latches the same register; both addresses read
 * back from the underlying low RAM, which is what the decoder does on the
 * real board. Bits [3:0] select the RAM bank, bits [5:4] the ROM bank.
 *
 * Every ROM bank carries an identical copy of the vectors and of the common
 * entry code, so the power-on state of the bank register does not matter.
 */

#include "emu.h"

#include "bus/rs232/rs232.h"
#include "cpu/m6502/w65c02s.h"
#include "imagedev/snapquik.h"
#include "machine/mc68681.h"
#include "machine/spi_sdcard.h"


namespace {

class rosco_6502_state : public driver_device
{
public:
	rosco_6502_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_duart(*this, "duart")
		, m_terminal(*this, "terminal")
		, m_host(*this, "host")
		, m_sdcard(*this, "sdcard")
		, m_rombank(*this, "rombank")
		, m_rambank(*this, "rambank")
		, m_lowram(*this, "lowram")
	{ }

	void rosco_6502(machine_config &config);

private:
	static constexpr int RAM_BANKS = 16;
	static constexpr int ROM_BANKS = 4;
	static constexpr offs_t RAM_BANK_SIZE = 0x8000;
	static constexpr offs_t ROM_BANK_SIZE = 0x2000;

	void mem_map(address_map &map) ATTR_COLD;

	DECLARE_QUICKLOAD_LOAD_MEMBER(quickload_cb);

	virtual void machine_start() override ATTR_COLD;
	virtual void machine_reset() override ATTR_COLD;

	uint8_t bank_r(offs_t offset);
	void bank_w(offs_t offset, uint8_t data);
	void update_banks();

	void write_red_led(int state);
	void write_green_led(int state);

	required_device<w65c02s_device> m_maincpu;
	required_device<xr68c681_device> m_duart;
	required_device<rs232_port_device> m_terminal;
	required_device<rs232_port_device> m_host;
	required_device<spi_sdcard_device> m_sdcard;

	required_memory_bank m_rombank;
	required_memory_bank m_rambank;
	required_shared_ptr<uint8_t> m_lowram;

	std::unique_ptr<uint8_t []> m_bankram;
	uint8_t m_bank_set = 0;
};


/******************************************************************************
 Address Map
******************************************************************************/

void rosco_6502_state::mem_map(address_map &map)
{
	map(0x0000, 0x3fff).ram().share("lowram");                                          /* 16KB low RAM */
	map(0x0000, 0x0001).rw(FUNC(rosco_6502_state::bank_r), FUNC(rosco_6502_state::bank_w)); /* bank register over RAM */
	map(0x4000, 0xbfff).bankrw(m_rambank);                                              /* 16 x 32KB RAM banks */
	map(0xc000, 0xc00f).rw(m_duart, FUNC(xr68c681_device::read), FUNC(xr68c681_device::write));
	map(0xe000, 0xffff).bankr(m_rombank);                                               /* 4 x 8KB ROM banks */
}

/******************************************************************************
 Input Ports
******************************************************************************/

static INPUT_PORTS_START( rosco_6502 )
INPUT_PORTS_END

/* Terminal default settings. */
static DEVICE_INPUT_DEFAULTS_START(terminal)
	DEVICE_INPUT_DEFAULTS( "RS232_RXBAUD", 0xff, RS232_BAUD_115200 )
	DEVICE_INPUT_DEFAULTS( "RS232_TXBAUD", 0xff, RS232_BAUD_115200 )
	DEVICE_INPUT_DEFAULTS( "RS232_DATABITS", 0xff, RS232_DATABITS_8 )
	DEVICE_INPUT_DEFAULTS( "RS232_PARITY", 0xff, RS232_PARITY_NONE )
	DEVICE_INPUT_DEFAULTS( "RS232_STOPBITS", 0xff, RS232_STOPBITS_1 )
DEVICE_INPUT_DEFAULTS_END

/******************************************************************************
 Banking
******************************************************************************/

uint8_t rosco_6502_state::bank_r(offs_t offset)
{
	// The bank register is write-only; reads come from the RAM underneath it.
	return m_lowram[offset];
}

void rosco_6502_state::bank_w(offs_t offset, uint8_t data)
{
	// BANKSEL is asserted for $0000 and $0001 alike (the decoder ignores A0),
	// so either address latches the register. The write still reaches RAM.
	m_lowram[offset] = data;

	m_bank_set = data;
	update_banks();
}

void rosco_6502_state::update_banks()
{
	m_rambank->set_entry(m_bank_set & 0x0f);
	m_rombank->set_entry((m_bank_set >> 4) & 0x03);
}

/******************************************************************************
 Machine Start/Reset
******************************************************************************/

void rosco_6502_state::machine_start()
{
	m_bankram = std::make_unique<uint8_t []>(RAM_BANKS * RAM_BANK_SIZE);

	m_rambank->configure_entries(0, RAM_BANKS, m_bankram.get(), RAM_BANK_SIZE);
	m_rombank->configure_entries(0, ROM_BANKS, memregion("rom")->base(), ROM_BANK_SIZE);

	save_pointer(NAME(m_bankram), RAM_BANKS * RAM_BANK_SIZE);
	save_item(NAME(m_bank_set));
}

void rosco_6502_state::machine_reset()
{
	m_sdcard->spi_clock_w(CLEAR_LINE);

	m_bank_set = 0;
	update_banks();
}

void rosco_6502_state::write_red_led(int state)
{
	// Nothing yet
}

void rosco_6502_state::write_green_led(int state)
{
	// Nothing yet
}

/******************************************************************************
 Machine Driver
******************************************************************************/

void rosco_6502_state::rosco_6502(machine_config &config)
{
	W65C02S(config, m_maincpu, 14_MHz_XTAL);
	m_maincpu->set_addrmap(AS_PROGRAM, &rosco_6502_state::mem_map);

	// The DUART handles both serial ports, the timer tick and, through its
	// input and output ports, the bit-banged SPI bus and the two LEDs.
	// IP0 = CTS_A     OP0 = RTS_A
	// IP1 = CTS_B     OP1 = RTS_B
	// IP2 = SPI_CIPO  OP2 = SPI_CS
	//                 OP3 = RED_LED (active low)
	//                 OP4 = SPI_SCK
	//                 OP5 = GREEN_LED (active low)
	//                 OP6 = SPI_COPI
	//                 OP7 = SPI_CS2

	XR68C681(config, m_duart, 3.6864_MHz_XTAL);
	m_duart->irq_cb().set_inputline(m_maincpu, w65c02s_device::IRQ_LINE);
	m_duart->a_tx_cb().set(m_terminal, FUNC(rs232_port_device::write_txd));
	m_duart->outport_cb().set(m_terminal, FUNC(rs232_port_device::write_rts)).bit(0);
	m_duart->b_tx_cb().set(m_host, FUNC(rs232_port_device::write_txd));
	m_duart->outport_cb().append(m_host, FUNC(rs232_port_device::write_rts)).bit(1);
	m_duart->outport_cb().append(FUNC(rosco_6502_state::write_red_led)).bit(3);
	m_duart->outport_cb().append(FUNC(rosco_6502_state::write_green_led)).bit(5);
	m_duart->outport_cb().append(m_sdcard, FUNC(spi_sdcard_device::spi_ss_w)).bit(2).invert();
	m_duart->outport_cb().append(m_sdcard, FUNC(spi_sdcard_device::spi_clock_w)).bit(4);
	m_duart->outport_cb().append(m_sdcard, FUNC(spi_sdcard_device::spi_mosi_w)).bit(6);

	RS232_PORT(config, m_terminal, default_rs232_devices, "terminal");
	m_terminal->rxd_handler().set(m_duart, FUNC(xr68c681_device::rx_a_w));
	m_terminal->set_option_device_input_defaults("terminal", DEVICE_INPUT_DEFAULTS_NAME(terminal));
	m_terminal->set_option_device_input_defaults("null_modem", DEVICE_INPUT_DEFAULTS_NAME(terminal));
	m_terminal->set_option_device_input_defaults("pty", DEVICE_INPUT_DEFAULTS_NAME(terminal));
	m_terminal->cts_handler().set(m_duart, FUNC(xr68c681_device::ip0_w));

	RS232_PORT(config, m_host, default_rs232_devices, nullptr);
	m_host->rxd_handler().set(m_duart, FUNC(xr68c681_device::rx_b_w));
	m_host->set_option_device_input_defaults("terminal", DEVICE_INPUT_DEFAULTS_NAME(terminal));
	m_host->set_option_device_input_defaults("null_modem", DEVICE_INPUT_DEFAULTS_NAME(terminal));
	m_host->set_option_device_input_defaults("pty", DEVICE_INPUT_DEFAULTS_NAME(terminal));
	m_host->cts_handler().set(m_duart, FUNC(xr68c681_device::ip1_w));

	SPI_SDCARD(config, m_sdcard, 0);
	m_sdcard->spi_miso_callback().set(m_duart, FUNC(xr68c681_device::ip2_w));

	// Drop a raw binary straight into memory instead of pasting Intel hex
	// into the monitor or putting it on the SD card. The delay lets the
	// firmware finish its power-on self test before control is handed over.
	QUICKLOAD(config, "quickload", "bin", attotime::from_seconds(3))
			.set_load_callback(FUNC(rosco_6502_state::quickload_cb));
}

/*
 * Programs are linked to load and start at $0800, the bottom of the user area
 * in low RAM. Everything below that belongs to the firmware.
 */
QUICKLOAD_LOAD_MEMBER(rosco_6502_state::quickload_cb)
{
	constexpr offs_t LOAD_ADDRESS = 0x0800;
	constexpr offs_t LOW_RAM_END = 0x4000;

	const uint64_t length = image.length();

	if (!length || (length > (LOW_RAM_END - LOAD_ADDRESS)))
		return std::make_pair(image_error::INVALIDLENGTH, std::string());

	std::vector<uint8_t> buffer(length);
	if (image.fread(&buffer[0], length) != length)
		return std::make_pair(image_error::UNSPECIFIED, "Error reading file");

	address_space &space = m_maincpu->space(AS_PROGRAM);
	for (offs_t i = 0; i < length; i++)
		space.write_byte(LOAD_ADDRESS + i, buffer[i]);

	// set_pc() goes through STATE_GENPC, which the m6502 core exports but
	// never imports; M6502_PC is the one that restarts the prefetch.
	m_maincpu->set_state_int(M6502_PC, LOAD_ADDRESS);

	return std::make_pair(std::error_condition(), std::string());
}

/******************************************************************************
 ROM Definition
******************************************************************************/

/*
 * As with the rosco_m68k, the firmware is built from source by its users, so
 * the hashes below just describe the build that ships in roms/. The image is
 * the four 8KB ROM banks concatenated in order.
 */
ROM_START( rosco_6502 )
	ROM_REGION(0x8000, "rom", 0)
	ROM_LOAD( "rosco_6502.rom", 0x0000, 0x8000, CRC(213c7751) SHA1(004ee5c6359e2f37a9e02c805ddfe713020838ae) )
ROM_END

} // anonymous namespace


/*    YEAR  NAME        PARENT  COMPAT  MACHINE     INPUT       CLASS             INIT        COMPANY                          FULLNAME      FLAGS */
COMP( 2022, rosco_6502, 0,      0,      rosco_6502, rosco_6502, rosco_6502_state, empty_init, "The Really Old-School Company", "rosco_6502", MACHINE_NO_SOUND_HW )
