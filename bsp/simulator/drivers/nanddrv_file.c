#include <rtdevice.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NAND_SIM  "nand.bin"
#if 1
#define OOB_SIZE		64
#define PAGE_SIZE		(2048 + 64)
#define PAGE_PER_BLOCK	64
#define BLOCK_SIZE		(PAGE_SIZE * PAGE_PER_BLOCK)
#define BLOCK_NUM		512
// #define BLOCK_NUM		2048
#else
#define OOB_SIZE		16
#define PAGE_SIZE		(512 + OOB_SIZE)
#define PAGE_PER_BLOCK	32
#define BLOCK_SIZE		(PAGE_SIZE * PAGE_PER_BLOCK)
#define BLOCK_NUM		512
#endif

static unsigned char block_data[BLOCK_SIZE];
static struct rt_mtd_nand_device _nanddrv_file_device;
static FILE* file = NULL;

static rt_uint8_t CountBitsInByte(rt_uint8_t byte)
{
	rt_uint8_t count = 0;

	while (byte > 0)
	{
		if (byte & 1)
		{
			count++;
		}
		byte >>= 1;
	}

	return count;
}

static void Compute256(const rt_uint8_t *data, rt_uint8_t *code)
{
    rt_uint32_t i;
    rt_uint8_t columnSum = 0;
    rt_uint8_t evenLineCode = 0;
    rt_uint8_t oddLineCode = 0;
    rt_uint8_t evenColumnCode = 0;
    rt_uint8_t oddColumnCode = 0;

    // Xor all bytes together to get the column sum;
    // At the same time, calculate the even and odd line codes
    for (i=0; i < 256; i++)
    {
        columnSum ^= data[i];

        // If the xor sum of the byte is 0, then this byte has no incidence on
        // the computed code; so check if the sum is 1.
        if ((CountBitsInByte(data[i]) & 1) == 1)
        {
            // Parity groups are formed by forcing a particular index bit to 0
            // (even) or 1 (odd).
            // Example on one byte:
            //
            // bits (dec)  7   6   5   4   3   2   1   0
            //      (bin) 111 110 101 100 011 010 001 000
            //                            '---'---'---'----------.
            //                                                   |
            // groups P4' ooooooooooooooo eeeeeeeeeeeeeee P4     |
            //        P2' ooooooo eeeeeee ooooooo eeeeeee P2     |
            //        P1' ooo eee ooo eee ooo eee ooo eee P1     |
            //                                                   |
            // We can see that:                                  |
            //  - P4  -> bit 2 of index is 0 --------------------'
            //  - P4' -> bit 2 of index is 1.
            //  - P2  -> bit 1 of index if 0.
            //  - etc...
            // We deduce that a bit position has an impact on all even Px if
            // the log2(x)nth bit of its index is 0
            //     ex: log2(4) = 2, bit2 of the index must be 0 (-> 0 1 2 3)
            // and on all odd Px' if the log2(x)nth bit of its index is 1
            //     ex: log2(2) = 1, bit1 of the index must be 1 (-> 0 1 4 5)
            //
            // As such, we calculate all the possible Px and Px' values at the
            // same time in two variables, evenLineCode and oddLineCode, such as
            //     evenLineCode bits: P128  P64  P32  P16  P8  P4  P2  P1
            //     oddLineCode  bits: P128' P64' P32' P16' P8' P4' P2' P1'
            //
            evenLineCode ^= (255 - i);
            oddLineCode ^= i;
        }
    }

    // At this point, we have the line parities, and the column sum. First, We
    // must caculate the parity group values on the column sum.
    for (i=0; i < 8; i++)
    {
        if (columnSum & 1)
        {
            evenColumnCode ^= (7 - i);
            oddColumnCode ^= i;
        }
        columnSum >>= 1;
    }

    // Now, we must interleave the parity values, to obtain the following layout:
    // Code[0] = Line1
    // Code[1] = Line2
    // Code[2] = Column
    // Line = Px' Px P(x-1)- P(x-1) ...
    // Column = P4' P4 P2' P2 P1' P1 PadBit PadBit
    code[0] = 0;
    code[1] = 0;
    code[2] = 0;

    for (i=0; i < 4; i++)
    {
        code[0] <<= 2;
        code[1] <<= 2;
        code[2] <<= 2;

        // Line 1
        if ((oddLineCode & 0x80) != 0)
        {
            code[0] |= 2;
        }

        if ((evenLineCode & 0x80) != 0)
        {
            code[0] |= 1;
        }

        // Line 2
        if ((oddLineCode & 0x08) != 0)
        {
            code[1] |= 2;
        }

        if ((evenLineCode & 0x08) != 0)
        {
            code[1] |= 1;
        }

        // Column
        if ((oddColumnCode & 0x04) != 0)
        {
            code[2] |= 2;
        }

        if ((evenColumnCode & 0x04) != 0)
        {
            code[2] |= 1;
        }

        oddLineCode <<= 1;
        evenLineCode <<= 1;
        oddColumnCode <<= 1;
        evenColumnCode <<= 1;
    }

    // Invert codes (linux compatibility)
    code[0] = (~(rt_uint32_t)code[0]);
    code[1] = (~(rt_uint32_t)code[1]);
    code[2] = (~(rt_uint32_t)code[2]);
}

void ecc_hamming_compute256x(const rt_uint8_t *pucData, rt_uint32_t dwSize, rt_uint8_t* puCode )
{
    while ( dwSize > 0 )
    {
        Compute256( pucData, puCode ) ;

        pucData += 256;
        puCode += 3;
        dwSize -= 256;
    }
}

/* read chip id */
static rt_uint32_t nanddrv_file_read_id(struct rt_mtd_nand_device* device)
{
	return 0x00;
}

/* read/write/move page */
static rt_err_t nanddrv_file_read_page(struct rt_mtd_nand_device* device,
									   rt_off_t page,
									   rt_uint8_t *data, rt_uint32_t data_len,
									   rt_uint8_t *spare, rt_uint32_t spare_len)
{
	rt_uint32_t offset;
	rt_uint8_t oob_buffer[OOB_SIZE];
	rt_uint8_t oob_ecc   [OOB_SIZE];

	page = page + device->block_start * device->pages_per_block;

	if (page/device->pages_per_block > device->block_end) 
	{
		return -RT_EIO;
	}

	/* write page */
	offset = page * PAGE_SIZE;
	if (data != NULL)
	{
		fseek(file, offset, SEEK_SET);
		fread(data, data_len, 1, file);
	}

	offset = page * PAGE_SIZE + (PAGE_SIZE - OOB_SIZE);
	fseek(file, offset, SEEK_SET);
	fread(oob_buffer, OOB_SIZE, 1, file);
	if (spare != NULL)
	{
		memcpy(spare, oob_buffer, spare_len);
	}

	/* verify ECC */
	if (data != RT_NULL)
	{
		ecc_hamming_compute256x(data, PAGE_SIZE - OOB_SIZE, &oob_ecc[0]);
		if (memcmp(&oob_ecc[0], &oob_buffer[0], OOB_SIZE - device->oob_free) != 0) return -RT_MTD_EECC;
	}

	return RT_EOK;
}

static rt_err_t nanddrv_file_write_page(struct rt_mtd_nand_device* device, 
										rt_off_t page, 
										const rt_uint8_t* data, rt_uint32_t data_len,
										const rt_uint8_t* oob, rt_uint32_t spare_len)
{
	rt_uint32_t offset;
	rt_uint8_t oob_buffer[OOB_SIZE];

	page = page + device->block_start * device->pages_per_block;
	if (page/device->pages_per_block > device->block_end) 
	{
		return -RT_EIO;
	}

	/* write page */
	offset = page * PAGE_SIZE;
	if (data != NULL)
	{
		fseek(file, offset, SEEK_SET);
		fwrite(data, PAGE_SIZE - OOB_SIZE, 1, file);
	}

	offset = page * PAGE_SIZE + (PAGE_SIZE - OOB_SIZE);
	fseek(file, offset, SEEK_SET);

	memset(oob_buffer, 0xff, sizeof(oob_buffer));
	ecc_hamming_compute256x(data, PAGE_SIZE - OOB_SIZE, &oob_buffer[0]);
	if (oob != RT_NULL) 
	{
		memcpy(&oob_buffer[OOB_SIZE - device->oob_free], 
			&oob[OOB_SIZE - device->oob_free], 
			device->oob_free);
	}
	fwrite(oob_buffer, OOB_SIZE, 1, file);

	return RT_EOK;
}

static rt_err_t nanddrv_file_move_page(struct rt_mtd_nand_device* device, rt_off_t from, rt_off_t to)
{
	rt_uint32_t offset;
	rt_uint8_t  page_buffer[PAGE_SIZE - OOB_SIZE];
	rt_uint8_t  oob_buffer[OOB_SIZE];

	from = from + device->block_start * device->pages_per_block;
	to = to + device->block_start * device->pages_per_block;

	if (from/device->pages_per_block > device->block_end ||
		to/device->pages_per_block > device->block_end) 
	{
		return -RT_EIO;
	}

	if (device->plane_num > 1)
	{
		rt_uint32_t mask;
		rt_uint16_t from_block, to_block;

		from_block = (rt_uint16_t)(from / PAGE_PER_BLOCK);
		to_block = (rt_uint16_t)(to / PAGE_PER_BLOCK);
		mask = device->plane_num - 1;

		if ((from_block & mask) != (to_block & mask))
		{
			rt_kprintf("invalid page copy on the block. from [%d] --> to[%d]\n", from_block, to_block);
			return -RT_EIO;
		}
	}

	/* read page */
	offset = from * PAGE_SIZE;
	fseek(file, offset, SEEK_SET);
	fread(page_buffer, sizeof(page_buffer), 1, file);
	fread(oob_buffer, sizeof(oob_buffer), 1, file);

	/* write page */
	offset = to * PAGE_SIZE;
	fseek(file, offset, SEEK_SET);
	fwrite(page_buffer, sizeof(page_buffer), 1, file);
	fwrite(oob_buffer, sizeof(oob_buffer), 1, file);

	return RT_EOK;
}

/* erase block */
static rt_err_t nanddrv_file_erase_block(struct rt_mtd_nand_device* device, rt_uint32_t block)
{
	if (block > BLOCK_NUM) return -RT_EIO;

	/* add the start blocks */
	block = block + device->block_start * device->pages_per_block;

	fseek(file, block * BLOCK_SIZE, SEEK_SET );
	fwrite(block_data, sizeof(block_data), 1, file);

	return RT_EOK;
}

const static struct rt_mtd_nand_driver_ops _ops = 
{
	nanddrv_file_read_id,
	nanddrv_file_read_page,
	nanddrv_file_write_page,
	nanddrv_file_move_page,
	nanddrv_file_erase_block
};

void nand_eraseall(void);

void rt_hw_mtd_nand_init(void)
{
	rt_uint16_t ecc_size;
	rt_uint32_t size;

	memset(block_data, 0xff, sizeof(block_data));
	/* open file */
	file = fopen(NAND_SIM, "rb+");
	if (file == NULL)
	{
		file = fopen(NAND_SIM, "wb+");
	}
	fseek(file, 0, SEEK_END);
	size = ftell(file);

	fseek(file, 0, SEEK_SET );
	if (size < BLOCK_NUM * BLOCK_SIZE)
	{
		rt_uint32_t index;
		fseek(file, 0, SEEK_SET );
		for (index = 0; index < BLOCK_NUM; index ++)
		{
			fwrite(block_data, sizeof(block_data), 1, file);
		}
	}
	fseek(file, 0, SEEK_SET );

	ecc_size = (PAGE_SIZE - OOB_SIZE) * 3/256;
	_nanddrv_file_device.plane_num = 2;
	_nanddrv_file_device.oob_size = OOB_SIZE;
	_nanddrv_file_device.oob_free = OOB_SIZE - ecc_size;
	_nanddrv_file_device.page_size = PAGE_SIZE - OOB_SIZE;
	_nanddrv_file_device.pages_per_block = PAGE_PER_BLOCK;
	_nanddrv_file_device.block_start = 0;
	_nanddrv_file_device.block_end = BLOCK_NUM/2;
	_nanddrv_file_device.block_total = _nanddrv_file_device.block_end - _nanddrv_file_device.block_start;
	_nanddrv_file_device.ops = &_ops;

	rt_mtd_nand_register_device("nand0", &_nanddrv_file_device);
}

#if defined(RT_USING_FINSH)
#include <finsh.h>
void nand_eraseall()
{
	int index;
	for (index = 0; index < _nanddrv_file_device.block_total; index ++)
	{
		nanddrv_file_erase_block(&_nanddrv_file_device, index);
	}
}
FINSH_FUNCTION_EXPORT(nand_eraseall, erase all of block in the nand flash);

#if 0
void nand_log(int level)
{
	nftl_set_trace_level(level);
}
FINSH_FUNCTION_EXPORT(nand_log, set NFTL trace level);
#endif 

#endif //RT_USING_FINSH
