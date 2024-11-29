/* $VER: unlzx.c 1.0 (22.2.98) */
/* Created: 11.2.98 */
/* Added Pipe support to read from stdin (03.4.01, Erik Meusel)           */

/* LZX Extract in (supposedly) portable C.                                */

/* Compile with:                                                          */
/* gcc unlzx.c -ounlzx -O6                                                */

/* Thanks to Dan Fraser for decoding the coredumps and helping me track   */
/* down some HIDEOUSLY ANNOYING bugs.                                     */

/* Everything is accessed as unsigned char's to try and avoid problems    */
/* with byte order and alignment. Most of the decrunch functions          */
/* encourage overruns in the buffers to make things as fast as possible.  */
/* All the time is taken up in crc::calc() and decrunch() so they are      */
/* pretty damn optimized. Don't try to understand this program.           */

/* ---------------------------------------------------------------------- */
#include "unlzx.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
// #include <unistd.h>
#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------------- */

static const unsigned char kVersion[] = "$VER: unlzx 1.1 (03.4.01)";

/* ---------------------------------------------------------------------- */

unsigned char info_header[10];
unsigned char archive_header[31];
unsigned char header_filename[256];
unsigned char header_comment[256];

unsigned int pack_size;
unsigned int unpack_size;

unsigned int  year, month, day;
unsigned int  hour, minute, second;
unsigned char attributes;
unsigned char pack_mode;

/* ---------------------------------------------------------------------- */

struct FilenameNode {
  struct FilenameNode* next;
  unsigned int         length;
  unsigned int         crc;
  char                 filename[256];
};

struct FilenameNode* filename_list;

/* ---------------------------------------------------------------------- */

unsigned char read_buffer[16384];                 /* have a reasonable sized read buffer */
unsigned char decrunch_buffer[258 + 65536 + 258]; /* allow overrun for speed */

unsigned char* source;
unsigned char* destination;
unsigned char* source_end;
unsigned char* destination_end;

/* ---------------------------------------------------------------------- */

static const char* month_str[16] = {"jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep",
    "oct", "nov", "dec", "?13", "?14", "?15", "?16"};

/* ---------------------------------------------------------------------- */


/* ---------------------------------------------------------------------- */

static const unsigned char kTableOne[32] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8,
    8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14};

static const unsigned int kTableTwo[32] = {0, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128,
    192, 256, 384, 512, 768, 1024, 1536, 2048, 3072, 4096, 6144, 8192, 12288, 16384, 24576, 32768,
    49152};

static const unsigned int kTableThree[16] = {
    0, 1, 3, 7, 15, 31, 63, 127, 255, 511, 1023, 2047, 4095, 8191, 16383, 32767};

static const unsigned char kTableFour[34] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    16, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

class HuffmanDecoder {
 public:
  HuffmanDecoder();
  int  read_literal_table();
  void decrunch();

  uint32_t decrunch_length() const {
    return decrunch_length_;
  }

 private:
  huffman::HuffmanTable offsets_;
  huffman::HuffmanTable huffman20_;
  huffman::HuffmanTable literals_;

  uint32_t global_control_{};
  int32_t  global_shift_{-16};
  uint32_t decrunch_method_{};
  uint32_t decrunch_length_{};

  uint32_t last_offset_{1};
};
/* ---------------------------------------------------------------------- */

/* ---------------------------------------------------------------------- */

/* Read and build the decrunch tables. There better be enough data in the */
/* source buffer or it's stuffed. */

HuffmanDecoder::HuffmanDecoder()
    : offsets_(7, 8, 128), huffman20_(6, 20, 96), literals_(12, 768, 5120) {}

int HuffmanDecoder::read_literal_table() {
  unsigned int control;
  int          shift;
  unsigned int temp; /* could be a register */
  unsigned int symbol, pos, count, fix, max_symbol;
  int          abort = 0;

  control = global_control_;
  shift   = global_shift_;

  if (shift < 0) { /* fix the control word if necessary */
    shift += 16;
    control += *source++ << (8 + shift);
    control += *source++ << shift;
  }

  /* read the decrunch method */
  decrunch_method_ = control & 7;
  control >>= 3;
  if ((shift -= 3) < 0) {
    shift += 16;
    control += *source++ << (8 + shift);
    control += *source++ << shift;
  }

  /* Read and build the offset huffman table */
  if ((!abort) && (decrunch_method_ == 3)) {
    for (temp = 0; temp < 8; temp++) {
      offsets_.bit_length_[temp] = control & 7;
      control >>= 3;
      if ((shift -= 3) < 0) {
        shift += 16;
        control += *source++ << (8 + shift);
        control += *source++ << shift;
      }
    }
    abort = !offsets_.reset_table();
  }

  /* read decrunch length */
  if (!abort) {
    decrunch_length_ = (control & 255) << 16;
    control >>= 8;
    if ((shift -= 8) < 0) {
      shift += 16;
      control += *source++ << (8 + shift);
      control += *source++ << shift;
    }
    decrunch_length_ += (control & 255) << 8;
    control >>= 8;
    if ((shift -= 8) < 0) {
      shift += 16;
      control += *source++ << (8 + shift);
      control += *source++ << shift;
    }
    decrunch_length_ += (control & 255);
    control >>= 8;
    if ((shift -= 8) < 0) {
      shift += 16;
      control += *source++ << (8 + shift);
      control += *source++ << shift;
    }
  }

  /* read and build the huffman literal table */
  if ((!abort) && (decrunch_method_ != 1)) {
    pos        = 0;
    fix        = 1;
    max_symbol = 256;

    do {
      for (temp = 0; temp < 20; temp++) {
        huffman20_.bit_length_[temp] = control & 15;
        control >>= 4;
        if ((shift -= 4) < 0) {
          shift += 16;
          control += *source++ << (8 + shift);
          control += *source++ << shift;
        }
      }
      abort = !huffman20_.reset_table();
      if (abort) break; /* argh! table is corrupt! */

      do {
        if ((symbol = huffman20_.table_[control & 63]) >= 20) {
          do { /* symbol is longer than 6 bits */
            symbol = huffman20_.table_[((control >> 6) & 1) + (symbol << 1)];
            if (!shift--) {
              shift += 16;
              control += *source++ << 24;
              control += *source++ << 16;
            }
            control >>= 1;
          } while (symbol >= 20);
          temp = 6;
        } else {
          temp = huffman20_.bit_length_[symbol];
        }
        control >>= temp;
        if ((shift -= temp) < 0) {
          shift += 16;
          control += *source++ << (8 + shift);
          control += *source++ << shift;
        }
        switch (symbol) {
        case 17:
        case 18: {
          if (symbol == 17) {
            temp  = 4;
            count = 3;
          } else { /* symbol == 18 */
            temp  = 6 - fix;
            count = 19;
          }
          count += (control & kTableThree[temp]) + fix;
          control >>= temp;
          if ((shift -= temp) < 0) {
            shift += 16;
            control += *source++ << (8 + shift);
            control += *source++ << shift;
          }
          while ((pos < max_symbol) && (count--)) literals_.bit_length_[pos++] = 0;
          break;
        }
        case 19: {
          count = (control & 1) + 3 + fix;
          if (!shift--) {
            shift += 16;
            control += *source++ << 24;
            control += *source++ << 16;
          }
          control >>= 1;
          if ((symbol = huffman20_.table_[control & 63]) >= 20) {
            do { /* symbol is longer than 6 bits */
              symbol = huffman20_.table_[((control >> 6) & 1) + (symbol << 1)];
              if (!shift--) {
                shift += 16;
                control += *source++ << 24;
                control += *source++ << 16;
              }
              control >>= 1;
            } while (symbol >= 20);
            temp = 6;
          } else {
            temp = huffman20_.bit_length_[symbol];
          }
          control >>= temp;
          if ((shift -= temp) < 0) {
            shift += 16;
            control += *source++ << (8 + shift);
            control += *source++ << shift;
          }
          symbol = kTableFour[literals_.bit_length_[pos] + 17 - symbol];
          while ((pos < max_symbol) && (count--)) literals_.bit_length_[pos++] = symbol;
          break;
        }
        default: {
          symbol                       = kTableFour[literals_.bit_length_[pos] + 17 - symbol];
          literals_.bit_length_[pos++] = symbol;
          break;
        }
        }
      } while (pos < max_symbol);
      fix--;
      max_symbol += 512;
    } while (max_symbol == 768);

    if (!abort) abort = !literals_.reset_table();
  }

  global_control_ = control;
  global_shift_   = shift;

  return (abort);
}

/* ---------------------------------------------------------------------- */

/* Fill up the decrunch buffer. Needs lots of overrun for both destination */
/* and source buffers. Most of the time is spent in this routine so it's  */
/* pretty damn optimized. */

void HuffmanDecoder::decrunch() {
  unsigned int   control;
  int            shift;
  unsigned int   temp; /* could be a register */
  unsigned int   symbol, count;
  unsigned char* string;

  control = global_control_;
  shift   = global_shift_;

  do {
    if ((symbol = literals_.table_[control & 4095]) >= 768) {
      control >>= 12;
      if ((shift -= 12) < 0) {
        shift += 16;
        control += *source++ << (8 + shift);
        control += *source++ << shift;
      }
      do { /* literal is longer than 12 bits */
        symbol = literals_.table_[(control & 1) + (symbol << 1)];
        if (!shift--) {
          shift += 16;
          control += *source++ << 24;
          control += *source++ << 16;
        }
        control >>= 1;
      } while (symbol >= 768);
    } else {
      temp = literals_.bit_length_[symbol];
      control >>= temp;
      if ((shift -= temp) < 0) {
        shift += 16;
        control += *source++ << (8 + shift);
        control += *source++ << shift;
      }
    }
    if (symbol < 256) {
      *destination++ = symbol;
    } else {
      symbol -= 256;
      count = kTableTwo[temp = symbol & 31];
      temp  = kTableOne[temp];
      if ((temp >= 3) && (decrunch_method_ == 3)) {
        temp -= 3;
        count += ((control & kTableThree[temp]) << 3);
        control >>= temp;
        if ((shift -= temp) < 0) {
          shift += 16;
          control += *source++ << (8 + shift);
          control += *source++ << shift;
        }
        count += (temp = offsets_.table_[control & 127]);
        temp = offsets_.bit_length_[temp];
      } else {
        count += control & kTableThree[temp];
        if (!count) count = last_offset_;
      }
      control >>= temp;
      if ((shift -= temp) < 0) {
        shift += 16;
        control += *source++ << (8 + shift);
        control += *source++ << shift;
      }
      last_offset_ = count;

      count = kTableTwo[temp = (symbol >> 5) & 15] + 3;
      temp  = kTableOne[temp];
      count += (control & kTableThree[temp]);
      control >>= temp;
      if ((shift -= temp) < 0) {
        shift += 16;
        control += *source++ << (8 + shift);
        control += *source++ << shift;
      }
      string = (decrunch_buffer + last_offset_ < destination) ? destination - last_offset_
                                                              : destination + 65536 - last_offset_;
      do {
        *destination++ = *string++;
      } while (--count);
    }
  } while ((destination < destination_end) && (source < source_end));

  global_control_ = control;
  global_shift_   = shift;
}

/* ---------------------------------------------------------------------- */

/* Opens a file for writing & creates the full path if required. */

FILE* open_output(char* filename) {
  unsigned int temp;
  FILE*        file;

  if (!(file = fopen(filename, "wb"))) {
    /* couldn't open the file. try and create directories */
    for (temp = 0; filename[temp]; temp++) {
      if (filename[temp] == '/') {
        filename[temp] = 0;
        mkdir(filename, 511); /* I don't care if it works or not */
        filename[temp] = '/';
      }
    }
    if (!(file = fopen(filename, "wb"))) {
      perror("FOpen");
    }
  }
  return (file);
}

/* ---------------------------------------------------------------------- */

/* Trying to understand this function is hazardous. */

int extract_normal(FILE* in_file) {
  struct FilenameNode* node;
  FILE*                out_file = 0;
  unsigned char*       pos;
  unsigned char*       temp;
  unsigned int         count;
  int                  abort = 0;

  HuffmanDecoder decoder;

  unpack_size             = 0;
  int64_t decrunch_length = 0;

  source_end = (source = read_buffer + 16384) - 1024;
  pos = destination_end = destination = decrunch_buffer + 258 + 65536;

  for (node = filename_list; (!abort) && node; node = node->next) {
    printf("Extracting \"%s\"...", node->filename);
    fflush(stdout);

    out_file = open_output(node->filename);

    crc::reset();

    unpack_size = node->length;

    while (unpack_size > 0) {
      if (pos == destination) {     /* time to fill the buffer? */
                                    /* check if we have enough data and read some if not */
        if (source >= source_end) { /* have we exhausted the current read buffer? */
          temp  = read_buffer;
          count = temp - source + 16384;
          if (count) {
            do { /* copy the remaining overrun to the start of the buffer */
              *temp++ = *source++;
            } while (--count);
          }
          source = read_buffer;
          count  = source - temp + 16384;

          if (pack_size < count) count = pack_size; /* make sure we don't read too much */

          if (fread(temp, 1, count, in_file) != count) {
            printf("\n");
            if (ferror(in_file))
              perror("FRead(Data)");
            else
              fprintf(stderr, "EOF: Data\n");
            abort = 1;
            break; /* fatal error */
          }
          pack_size -= count;

          temp += count;
          if (source >= temp) break; /* argh! no more data! */
        }

        /* if(source >= source_end) */
        /* check if we need to read the tables */
        if (decrunch_length <= 0) {
          if (decoder.read_literal_table()) break; /* argh! can't make huffman tables! */
          decrunch_length = decoder.decrunch_length();
        }

        /* unpack some data */
        if (destination >= decrunch_buffer + 258 + 65536) {
          count = destination - decrunch_buffer - 65536;
          if (count) {
            temp = (destination = decrunch_buffer) + 65536;
            do { /* copy the overrun to the start of the buffer */
              *destination++ = *temp++;
            } while (--count);
          }
          pos = destination;
        }
        destination_end = destination + decrunch_length;
        if (destination_end > decrunch_buffer + 258 + 65536)
          destination_end = decrunch_buffer + 258 + 65536;
        temp = destination;

        decoder.decrunch();

        decrunch_length -= (destination - temp);
      }

      /* calculate amount of data we can use before we need to fill the buffer again */
      count = destination - pos;
      if (count > unpack_size) count = unpack_size; /* take only what we need */

      crc::calc(pos, count);

      if (out_file) { /* Write the data to the file */
        if (fwrite(pos, 1, count, out_file) != count) {
          perror("FWrite"); /* argh! write error */
          fclose(out_file);
          out_file = 0;
        }
      }
      unpack_size -= count;
      pos += count;
    }

    if (out_file) {
      fclose(out_file);
      if (!abort) printf(" crc %s\n", (node->crc == crc::sum()) ? "good" : "bad");
    }
  } /* for */

  return (abort);
}

/* ---------------------------------------------------------------------- */

/* This is less complex than extract_normal. Almost decipherable. */

int extract_store(FILE* in_file) {
  struct FilenameNode* node;
  FILE*                out_file;
  unsigned int         count;
  int                  abort = 0;

  for (node = filename_list; (!abort) && node; node = node->next) {
    printf("Storing \"%s\"...", node->filename);
    fflush(stdout);

    out_file = open_output(node->filename);

    crc::reset();

    unpack_size = node->length;
    if (unpack_size > pack_size) unpack_size = pack_size;

    while (unpack_size > 0) {
      count = (unpack_size > 16384) ? 16384 : unpack_size;

      if (fread(read_buffer, 1, count, in_file) != count) {
        printf("\n");
        if (ferror(in_file))
          perror("FRead(Data)");
        else
          fprintf(stderr, "EOF: Data\n");
        abort = 1;
        break; /* fatal error */
      }
      pack_size -= count;

      crc::calc(read_buffer, count);

      if (out_file) { /* Write the data to the file */
        if (fwrite(read_buffer, 1, count, out_file) != count) {
          perror("FWrite"); /* argh! write error */
          fclose(out_file);
          out_file = 0;
        }
      }
      unpack_size -= count;
    }

    if (out_file) {
      fclose(out_file);
      if (!abort) printf(" crc %s\n", (node->crc == crc::sum()) ? "good" : "bad");
    }
  } /* for */

  return (abort);
}

/* ---------------------------------------------------------------------- */

/* Easiest of the three. Just print the file(s) we didn't understand. */

int extract_unknown(FILE* in_file) {
  struct FilenameNode* node;
  int                  abort = 0;

  for (node = filename_list; node; node = node->next) {
    printf("Unknown \"%s\"\n", node->filename);
  }

  return (abort);
}

/* ---------------------------------------------------------------------- */

/* Read the archive and build a linked list of names. Merged files is     */
/* always assumed. Will fail if there is no memory for a node. Sigh.      */

int extract_archive(FILE* in_file) {
  unsigned int          temp;
  struct FilenameNode** filename_next;
  struct FilenameNode*  node;
  struct FilenameNode*  temp_node;
  int                   actual;
  int                   abort;
  int                   result = 1; /* assume an error */

  filename_list = 0; /* clear the list */
  filename_next = &filename_list;

  do {
    abort  = 1; /* assume an error */
    actual = fread(archive_header, 1, 31, in_file);
    if (!ferror(in_file)) {
      if (actual) { /* 0 is normal and means EOF */
        if (actual == 31) {
          crc::reset();
          uint32_t item_crc = (archive_header[29] << 24) + (archive_header[28] << 16)
                              + (archive_header[27] << 8) + archive_header[26]; /* header crc */
          archive_header[29] = 0; /* Must set the field to 0 before calculating the crc */
          archive_header[28] = 0;
          archive_header[27] = 0;
          archive_header[26] = 0;
          crc::calc(archive_header, 31);
          temp   = archive_header[30]; /* filename length */
          actual = fread(header_filename, 1, temp, in_file);
          if (!ferror(in_file)) {
            if (actual == temp) {
              header_filename[temp] = 0;
              crc::calc(header_filename, temp);
              temp   = archive_header[14]; /* comment length */
              actual = fread(header_comment, 1, temp, in_file);
              if (!ferror(in_file)) {
                if (actual == temp) {
                  header_comment[temp] = 0;
                  crc::calc(header_comment, temp);
                  if (crc::sum() == item_crc) {
                    unpack_size = (archive_header[5] << 24) + (archive_header[4] << 16)
                                  + (archive_header[3] << 8) + archive_header[2]; /* unpack size */
                    pack_size = (archive_header[9] << 24) + (archive_header[8] << 16)
                                + (archive_header[7] << 8) + archive_header[6]; /* packed size */
                    pack_mode = archive_header[11];                             /* pack mode */
                    item_crc  = (archive_header[25] << 24) + (archive_header[24] << 16)
                               + (archive_header[23] << 8) + archive_header[22]; /* data crc */

                    /* allocate a filename node */
                    node = (struct FilenameNode*)malloc(sizeof(struct FilenameNode));
                    if (node) {
                      *filename_next = node; /* add this node to the list */
                      filename_next  = &(node->next);
                      node->next     = 0;
                      node->length   = unpack_size;
                      node->crc      = item_crc;

                      snprintf(node->filename, sizeof node->filename, "%s", header_filename);

                      if (pack_size) {
                        switch (pack_mode) {
                        case 0: /* store */
                        {
                          abort = extract_store(in_file);
                          break;
                        }
                        case 2: /* normal */
                        {
                          abort = extract_normal(in_file);
                          break;
                        }
                        default: /* unknown */
                        {
                          abort = extract_unknown(in_file);
                          break;
                        }
                        }
                        if (abort) break; /* a read error occured */

                        temp_node = filename_list; /* free the list now */
                        while ((node = temp_node)) {
                          temp_node = node->next;
                          free(node);
                        }
                        filename_list = 0; /* clear the list */
                        filename_next = &filename_list;

                        if (fseek(in_file, pack_size, SEEK_CUR)) {
                          perror("FSeek(Data)");
                          break;
                        }
                      } else
                        abort = 0; /* continue */
                    } else
                      fprintf(stderr, "MAlloc(Filename_node)\n");
                  } else
                    fprintf(stderr, "CRC: Archive_Header\n");
                } else
                  fprintf(stderr, "EOF: Header_Comment\n");
              } else
                perror("FRead(Header_Comment)");
            } else
              fprintf(stderr, "EOF: Header_Filename\n");
          } else
            perror("FRead(Header_Filename)");
        } else
          fprintf(stderr, "EOF: Archive_Header\n");
      } else {
        result = 0; /* normal termination */
      }
    } else
      perror("FRead(Archive_Header)");
  } while (!abort);

  /* free the filename list in case an error occured */
  temp_node = filename_list;
  while ((node = temp_node)) {
    temp_node = node->next;
    free(node);
  }

  return (result);
}

/* ---------------------------------------------------------------------- */

/* List the contents of an archive in a nice formatted kinda way.         */

int view_archive(FILE* in_file) {
  unsigned int temp;
  unsigned int total_pack   = 0;
  unsigned int total_unpack = 0;
  unsigned int total_files  = 0;
  unsigned int merge_size   = 0;
  int          actual;
  int          abort;
  int          result = 1; /* assume an error */

  printf("Unpacked   Packed Time     Date        Attrib   Name\n");
  printf("-------- -------- -------- ----------- -------- ----\n");

  do {
    abort  = 1; /* assume an error */
    actual = fread(archive_header, 1, 31, in_file);
    if (!ferror(in_file)) {
      if (actual) { /* 0 is normal and means EOF */
        if (actual == 31) {
          crc::reset();
          uint32_t item_crc = (archive_header[29] << 24) + (archive_header[28] << 16)
                              + (archive_header[27] << 8) + archive_header[26];
          archive_header[29] = 0; /* Must set the field to 0 before calculating the crc */
          archive_header[28] = 0;
          archive_header[27] = 0;
          archive_header[26] = 0;
          crc::calc(archive_header, 31);
          temp   = archive_header[30]; /* filename length */
          actual = fread(header_filename, 1, temp, in_file);
          if (!ferror(in_file)) {
            if (actual == temp) {
              header_filename[temp] = 0;
              crc::calc(header_filename, temp);
              temp   = archive_header[14]; /* comment length */
              actual = fread(header_comment, 1, temp, in_file);
              if (!ferror(in_file)) {
                if (actual == temp) {
                  header_comment[temp] = 0;
                  crc::calc(header_comment, temp);
                  if (crc::sum() == item_crc) {
                    attributes  = archive_header[0]; /* file protection modes */
                    unpack_size = (archive_header[5] << 24) + (archive_header[4] << 16)
                                  + (archive_header[3] << 8) + archive_header[2]; /* unpack size */
                    pack_size = (archive_header[9] << 24) + (archive_header[8] << 16)
                                + (archive_header[7] << 8) + archive_header[6]; /* packed size */
                    temp = (archive_header[18] << 24) + (archive_header[19] << 16)
                           + (archive_header[20] << 8) + archive_header[21]; /* date */
                    year   = ((temp >> 17) & 63) + 1970;
                    month  = (temp >> 23) & 15;
                    day    = (temp >> 27) & 31;
                    hour   = (temp >> 12) & 31;
                    minute = (temp >> 6) & 63;
                    second = temp & 63;

                    total_pack += pack_size;
                    total_unpack += unpack_size;
                    total_files++;
                    merge_size += unpack_size;

                    printf("%8ld ", unpack_size);
                    if (archive_header[12] & 1)
                      printf("     n/a ");
                    else
                      printf("%8ld ", pack_size);
                    printf("%02ld:%02ld:%02ld ", hour, minute, second);
                    printf("%2ld-%s-%4ld ", day, month_str[month], year);
                    printf("%c%c%c%c%c%c%c%c ", (attributes & 32) ? 'h' : '-',
                        (attributes & 64) ? 's' : '-', (attributes & 128) ? 'p' : '-',
                        (attributes & 16) ? 'a' : '-', (attributes & 1) ? 'r' : '-',
                        (attributes & 2) ? 'w' : '-', (attributes & 8) ? 'e' : '-',
                        (attributes & 4) ? 'd' : '-');
                    printf("\"%s\"\n", header_filename);
                    if (header_comment[0]) printf(": \"%s\"\n", header_comment);
                    if ((archive_header[12] & 1) && pack_size) {
                      printf("%8ld %8ld Merged\n", merge_size, pack_size);
                    }

                    if (pack_size) { /* seek past the packed data */
                      merge_size = 0;
                      if (!fseek(in_file, pack_size, SEEK_CUR)) {
                        abort = 0; /* continue */
                      } else
                        perror("FSeek()");
                    } else
                      abort = 0; /* continue */
                  } else
                    fprintf(stderr, "CRC: Archive_Header\n");
                } else
                  fprintf(stderr, "EOF: Header_Comment\n");
              } else
                perror("FRead(Header_Comment)");
            } else
              fprintf(stderr, "EOF: Header_Filename\n");
          } else
            perror("FRead(Header_Filename)");
        } else
          fprintf(stderr, "EOF: Archive_Header\n");
      } else {
        printf("-------- -------- -------- ----------- -------- ----\n");
        printf("%8ld %8ld ", total_unpack, total_pack);
        printf("%ld file%s\n", total_files, ((total_files == 1) ? "" : "s"));

        result = 0; /* normal termination */
      }
    } else
      perror("FRead(Archive_Header)");
  } while (!abort);

  return (result);
}

/* ---------------------------------------------------------------------- */

/* Process a single archive. */

int process_archive(char* filename, Action action) {
  int   result = 1; /* assume an error */
  FILE* in_file;
  int   actual;

  if (NULL == filename)
    in_file = stdin;
  else if (NULL == (in_file = fopen(filename, "rb"))) {
    perror("FOpen(Archive)");
    return (result);
  }

  actual = fread(info_header, 1, 10, in_file);
  if (!ferror(in_file)) {
    if (actual == 10) {
      if ((info_header[0] == 76) && (info_header[1] == 90) && (info_header[2] == 88)) { /* LZX */
        switch (action) {
        case Action::Extract:
          result = extract_archive(in_file);
          break;

        case Action::View:
          result = view_archive(in_file);
          break;
        }
      } else
        fprintf(stderr, "Info_Header: Bad ID\n");
    } else
      fprintf(stderr, "EOF: Info_Header\n");
  } else
    perror("FRead(Info_Header)");
  fclose(in_file);

  return (result);
}

/* ---------------------------------------------------------------------- */

/* Some info for the reader only. This is unused by the program and can   */
/* safely be deleted.                                                     */

#define INFO_DAMAGE_PROTECT 1
#define INFO_FLAG_LOCKED 2

/* STRUCTURE Info_Header
{
  UBYTE ID[3]; 0 - "LZX"
  UBYTE flags; 3 - INFO_FLAG_#?
  UBYTE[6]; 4
     } *//* SIZE = 10 */

#define HDR_FLAG_MERGED 1

#define HDR_PROT_READ 1
#define HDR_PROT_WRITE 2
#define HDR_PROT_DELETE 4
#define HDR_PROT_EXECUTE 8
#define HDR_PROT_ARCHIVE 16
#define HDR_PROT_HOLD 32
#define HDR_PROT_SCRIPT 64
#define HDR_PROT_PURE 128

#define HDR_TYPE_MSDOS 0
#define HDR_TYPE_WINDOWS 1
#define HDR_TYPE_OS2 2
#define HDR_TYPE_AMIGA 10
#define HDR_TYPE_UNIX 20

#define HDR_PACK_STORE 0
#define HDR_PACK_NORMAL 2
#define HDR_PACK_EOF 32

/* STRUCTURE Archive_Header
{
  UBYTE attributes; 0 - HDR_PROT_#?
  UBYTE; 1
  ULONG unpacked_length; 2 - FUCKED UP LITTLE ENDIAN SHIT
  ULONG packed_length; 6 - FUCKED UP LITTLE ENDIAN SHIT
  UBYTE machine_type; 10 - HDR_TYPE_#?
  UBYTE pack_mode; 11 - HDR_PACK_#?
  UBYTE flags; 12 - HDR_FLAG_#?
  UBYTE; 13
  UBYTE len_comment; 14 - comment length [0,79]
  UBYTE extract_ver; 15 - version needed to extract
  UBYTE; 16
  UBYTE; 17
  ULONG date; 18 - Packed_Date
  ULONG data_crc; 22 - FUCKED UP LITTLE ENDIAN SHIT
  ULONG header_crc; 26 - FUCKED UP LITTLE ENDIAN SHIT
  UBYTE filename_len; 30 - filename length
     } *//* SIZE = 31 */

#define DATE_SHIFT_YEAR 17
#define DATE_SHIFT_MONTH 23
#define DATE_SHIFT_DAY 27
#define DATE_SHIFT_HOUR 12
#define DATE_SHIFT_MINUTE 6
#define DATE_SHIFT_SECOND 0
#define DATE_MASK_YEAR 0x007E0000
#define DATE_MASK_MONTH 0x07800000
#define DATE_MASK_DAY 0xF8000000
#define DATE_MASK_HOUR 0x0001F000
#define DATE_MASK_MINUTE 0x00000FC0
#define DATE_MASK_SECOND 0x0000003F

/* STRUCTURE DATE_Unpacked
{
  UBYTE year; 80 - Year 0=1970 1=1971 63=2033
  UBYTE month; 81 - 0=january 1=february .. 11=december
  UBYTE day; 82
  UBYTE hour; 83
  UBYTE minute; 84
  UBYTE second; 85
     } *//* SIZE = 6 */

/* STRUCTURE DATE_Packed
{
  UBYTE packed[4]; bit 0 is MSB, 31 is LSB
; bit # 0-4=Day 5-8=Month 9-14=Year 15-19=Hour 20-25=Minute 26-31=Second
     } *//* SIZE = 4 */
