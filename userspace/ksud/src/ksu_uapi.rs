#![allow(nonstandard_style, unused, unsafe_op_in_unsafe_fn)]
#![allow(clippy::all, clippy::pedantic, clippy::nursery)]
include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

#[derive(Clone, Debug)]
pub struct ProcessTag {
    pub tag_type: u8,
    pub name: String,
}
