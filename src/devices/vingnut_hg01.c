#include "decoder.h"

#define HG01_DEVICE_BITLEN      65
#define HG01_MESSAGE_BITLEN     65
#define HG01_MESSAGE_BYTELEN    (HG01_MESSAGE_BITLEN + 7) / 8
#define HG01_DEVICE_MINREPEATS  3

static const uint8_t preamble_pattern[] = {0x55, 0xaa, 0xde};

static int decode_hg01_message(r_device *decoder, bitbuffer_t *bitbuffer,
        unsigned row, uint16_t bitpos, data_t **data)
{
  uint8_t b[HG01_MESSAGE_BYTELEN];
  char id[4] = {0};
  int8_t tempc;
  uint8_t moisture;
  uint8_t light;
  uint8_t battery;

  // Extract the message; assigned to b
  bitbuffer_invert(bitbuffer);
  bitbuffer_extract_bytes(bitbuffer, row, bitpos, b, HG01_MESSAGE_BITLEN);

  // Extract the id as hex string
  snprintf(id, sizeof(id), "%02x", b[3]);
  moisture = b[4];
  uint8_t temp_raw = b[5];
  if (temp_raw & 0x80) {
    tempc = (temp_raw & 0x7f) * -1;
  } else {
    tempc = temp_raw;
  }
  battery = (b[6] >> 4);
  light = (b[6] << 4) | (b[7] >> 4);
  *data = data_make(
    "model",            "model",         DATA_STRING, "Vingnut HG01",
    "id",               "id",            DATA_STRING, id,
    "temperature_C",      "Temperature",   DATA_FORMAT, "%d C", DATA_INT, tempc,
    "soil_moisture",    "Soil Moisture", DATA_FORMAT, "%u %%", DATA_INT, moisture,
    "light",            "Light",         DATA_INT, light,
    "battery",          "Battery Bars",  DATA_INT, battery,
    NULL);
  return 1;
}

static int hg01_callback(r_device *decoder, bitbuffer_t *bitbuffer)
{
    int row; // a row index
    uint8_t b[HG01_MESSAGE_BYTELEN];
    uint16_t bitpos;
    int events = 0;
    int ret    = 0;
    char id[4] = {0};
    data_t *data = NULL;

    decoder_log_bitbuffer(decoder, 2, __func__, bitbuffer, "hex(/binary) version of bitbuffer");

    for (row = 0; row < bitbuffer->num_rows; ++row) {
        if (bitbuffer->bits_per_row[row] < HG01_MESSAGE_BITLEN) {
            // bail out of this row early
            decoder_logf_bitrow(decoder, 1, __func__, bitbuffer->bb[row], bitbuffer->bits_per_row[row],
                    "Bad message need %d bits got %d, row %d bit %d",
                    HG01_MESSAGE_BITLEN, bitbuffer->bits_per_row[row], row, 0);
            continue;
        }
        // We have enough bits so search for a message preamble followed by
        // enough bits that it could be a complete message.
        bitpos = 0;
        while ((bitpos = bitbuffer_search(bitbuffer, row, bitpos, preamble_pattern, sizeof(preamble_pattern) * 8)) + HG01_MESSAGE_BITLEN <= 
                bitbuffer->bits_per_row[row]) {
            ret = decode_hg01_message(decoder, bitbuffer, row, bitpos, &data);
            if (ret > 0) {
                events += ret;
                decoder_output_data(decoder, data);
            }
            bitpos += HG01_MESSAGE_BITLEN;
        }
    }
    return events > 0 ? events : ret;
}

r_device const vingnut_hg01 = {
    .name           = "Vingnut Moisture Meter",
    .modulation     = OOK_PULSE_PWM,
    .short_width    = 380,
    .long_width     = 1180,
    .reset_limit    = 4076,
    .gap_limit      = 1308,
    .tolerance      = 320,
    .decode_fn      = &hg01_callback,
    .disabled       = 0,
};
