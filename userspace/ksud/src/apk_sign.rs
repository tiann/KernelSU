use anyhow::{ensure, Result};
use std::io::{Read, Seek, SeekFrom};

pub fn get_apk_signature(apk: &str) -> Result<(u32, u32)> {
    let mut buffer = [0u8; 0x10];
    let mut size4 = [0u8; 4];
    let mut size8 = [0u8; 8];
    let mut size_of_block = [0u8; 8];

    let mut f = std::fs::File::open(apk)?;

    let mut i = 0;
    loop {
        let mut n = [0u8; 2];
        f.seek(SeekFrom::End(-i - 2))?;
        f.read_exact(&mut n)?;

        let n = u16::from_le_bytes(n);
        if i64::from(n) == i {
            f.seek(SeekFrom::Current(-22))?;
            f.read_exact(&mut size4)?;

            if u32::from_le_bytes(size4) ^ 0xcafe_babe_u32 == 0xccfb_f1ee_u32 {
                if i > 0 {
                    println!("warning: comment length is {i}");
                }
                break;
            }
        }

        ensure!(n != 0xffff, "not a zip file");

        i += 1;
    }

    f.seek(SeekFrom::Current(12))?;
    // offset
    f.read_exact(&mut size4)?;
    f.seek(SeekFrom::Start(u64::from(u32::from_le_bytes(size4)) - 0x18))?;

    f.read_exact(&mut size8)?;
    f.read_exact(&mut buffer)?;

    ensure!(&buffer == b"APK Sig Block 42", "Can not found sig block");

    let pos = u64::from(u32::from_le_bytes(size4)) - (u64::from_le_bytes(size8) + 0x8);
    f.seek(SeekFrom::Start(pos))?;
    f.read_exact(&mut size_of_block)?;

    ensure!(size_of_block == size8, "not a signed apk");

    loop {
        let mut id = [0u8; 4];
        let offset = 4u32;

        f.read_exact(&mut size8)?; // sequence length
        if size8 == size_of_block {
            break;
        }

        f.read_exact(&mut id)?; // id

        let id = u32::from_le_bytes(id);
        if (id ^ 0xdead_beef_u32) == 0xafa4_39f5_u32 || (id ^ 0xdead_beef_u32) == 0x2efe_d62f_u32 {
            f.read_exact(&mut size4)?; // signer-sequence length
            f.read_exact(&mut size4)?; // signer length
            f.read_exact(&mut size4)?; // signed data length
                                       // offset += 0x4 * 3;

            f.read_exact(&mut size4)?; // digests-sequcence length
            let pos = u32::from_le_bytes(size4);
            f.seek(SeekFrom::Current(i64::from(pos)))?;
            // offset += 0x4 + pos;

            f.read_exact(&mut size4)?; // certificates length
            f.read_exact(&mut size4)?; // certificate length
                                       // offset += 0x4 * 2;

            let mut hash = 1i32;
            let mut c = [0u8; 1];

            let j = u32::from_le_bytes(size4);
            for _ in 0..j {
                f.read_exact(&mut c)?;
                hash = hash.wrapping_mul(31).wrapping_add(i32::from(c[0] as i8));
            }

            // offset += j;

            let out_size = j;
            let out_hash = (hash as u32) ^ 0x1413_1211_u32;

            return Ok((out_size, out_hash));
        }

        f.seek(SeekFrom::Current(
            i64::from_le_bytes(size8) - i64::from(offset),
        ))?;
    }

    Err(anyhow::anyhow!("Unknown error"))
}
