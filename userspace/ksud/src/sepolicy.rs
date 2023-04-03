use anyhow::{bail, Result};
use derive_new::new;
use nom::{
    branch::alt,
    bytes::complete::{tag, take_while, take_while1, take_while_m_n},
    character::{
        complete::{space0, space1},
        is_alphanumeric,
    },
    combinator::map,
    sequence::Tuple,
    IResult, Parser,
};
use std::{path::Path, vec};

type SeObject<'a> = Vec<&'a str>;

fn is_sepolicy_char(c: char) -> bool {
    is_alphanumeric(c as u8) || c == '_' || c == '-'
}

fn parse_single_word(input: &str) -> IResult<&str, &str> {
    take_while1(is_sepolicy_char).parse(input)
}

fn parse_bracket_objs(input: &str) -> IResult<&str, SeObject> {
    let (input, (_, words, _)) = (
        tag("{"),
        take_while_m_n(1, 100, |c: char| is_sepolicy_char(c) || c.is_whitespace()),
        tag("}"),
    )
        .parse(input)?;
    Ok((input, words.split_whitespace().collect()))
}

fn parse_single_obj(input: &str) -> IResult<&str, SeObject> {
    let (input, word) = take_while1(is_sepolicy_char).parse(input)?;
    Ok((input, vec![word]))
}

fn parse_star(input: &str) -> IResult<&str, SeObject> {
    let (input, _) = tag("*").parse(input)?;
    Ok((input, vec!["*"]))
}

// 1. a single sepolicy word
// 2. { obj1 obj2 obj3 ...}
// 3. *
fn parse_seobj(input: &str) -> IResult<&str, SeObject> {
    let (input, strs) = alt((parse_single_obj, parse_bracket_objs, parse_star)).parse(input)?;
    Ok((input, strs))
}

fn parse_seobj_no_star(input: &str) -> IResult<&str, SeObject> {
    let (input, strs) = alt((parse_single_obj, parse_bracket_objs)).parse(input)?;
    Ok((input, strs))
}

trait SeObjectParser<'a> {
    fn parse(input: &'a str) -> IResult<&'a str, Self>
    where
        Self: Sized;
}

#[derive(Debug, PartialEq, Eq, new)]
struct NormalPerm<'a> {
    op: &'a str,
    source: SeObject<'a>,
    target: SeObject<'a>,
    class: SeObject<'a>,
    perm: SeObject<'a>,
}

#[derive(Debug, PartialEq, Eq, new)]
struct XPerm<'a> {
    op: &'a str,
    source: SeObject<'a>,
    target: SeObject<'a>,
    class: SeObject<'a>,
    operation: &'a str,
    perm_set: &'a str,
}

#[derive(Debug, PartialEq, Eq, new)]
struct TypeState<'a> {
    op: &'a str,
    stype: SeObject<'a>,
}

#[derive(Debug, PartialEq, Eq, new)]
struct TypeAttr<'a> {
    stype: SeObject<'a>,
    sattr: SeObject<'a>,
}

#[derive(Debug, PartialEq, Eq, new)]
struct Type<'a> {
    name: &'a str,
    attrs: SeObject<'a>,
}

#[derive(Debug, PartialEq, Eq, new)]
struct Attr<'a> {
    name: &'a str,
}

#[derive(Debug, PartialEq, Eq, new)]
struct TypeTransition<'a> {
    source: &'a str,
    target: &'a str,
    class: &'a str,
    default_type: &'a str,
    object_name: Option<&'a str>,
}

#[derive(Debug, PartialEq, Eq, new)]
struct TypeChange<'a> {
    op: &'a str,
    source: &'a str,
    target: &'a str,
    class: &'a str,
    default_type: &'a str,
}

#[derive(Debug, PartialEq, Eq, new)]
struct GenFsCon<'a> {
    fs_name: &'a str,
    partial_path: &'a str,
    fs_context: &'a str,
}

#[derive(Debug)]
enum PolicyStatement<'a> {
    // "allow *source_type *target_type *class *perm_set"
    // "deny *source_type *target_type *class *perm_set"
    // "auditallow *source_type *target_type *class *perm_set"
    // "dontaudit *source_type *target_type *class *perm_set"
    NormalPerm(NormalPerm<'a>),

    // "allowxperm *source_type *target_type *class operation xperm_set"
    // "auditallowxperm *source_type *target_type *class operation xperm_set"
    // "dontauditxperm *source_type *target_type *class operation xperm_set"
    XPerm(XPerm<'a>),

    // "permissive ^type"
    // "enforce ^type"
    TypeState(TypeState<'a>),

    // "type type_name ^(attribute)"
    Type(Type<'a>),

    // "typeattribute ^type ^attribute"
    TypeAttr(TypeAttr<'a>),

    // "attribute ^attribute"
    Attr(Attr<'a>),

    // "type_transition source_type target_type class default_type (object_name)"
    TypeTransition(TypeTransition<'a>),

    // "type_change source_type target_type class default_type"
    // "type_member source_type target_type class default_type"
    TypeChange(TypeChange<'a>),

    // "genfscon fs_name partial_path fs_context"
    GenFsCon(GenFsCon<'a>),
}

impl<'a> SeObjectParser<'a> for NormalPerm<'a> {
    fn parse(input: &'a str) -> IResult<&str, Self> {
        let (input, op) = alt((
            tag("allow"),
            tag("deny"),
            tag("auditallow"),
            tag("dontaudit"),
        ))(input)?;

        let (input, _) = space0(input)?;
        let (input, source) = parse_seobj(input)?;
        let (input, _) = space0(input)?;
        let (input, target) = parse_seobj(input)?;
        let (input, _) = space0(input)?;
        let (input, class) = parse_seobj(input)?;
        let (input, _) = space0(input)?;
        let (input, perm) = parse_seobj(input)?;
        Ok((input, NormalPerm::new(op, source, target, class, perm)))
    }
}

impl<'a> SeObjectParser<'a> for XPerm<'a> {
    fn parse(input: &'a str) -> IResult<&'a str, Self> {
        let (input, op) = alt((
            tag("allowxperm"),
            tag("auditallowxperm"),
            tag("dontauditxperm"),
        ))(input)?;

        let (input, _) = space0(input)?;
        let (input, source) = parse_seobj(input)?;
        let (input, _) = space0(input)?;
        let (input, target) = parse_seobj(input)?;
        let (input, _) = space0(input)?;
        let (input, class) = parse_seobj(input)?;
        let (input, _) = space0(input)?;
        let (input, operation) = parse_single_word(input)?;
        let (input, _) = space0(input)?;
        let (input, perm_set) = parse_single_word(input)?;

        Ok((
            input,
            XPerm::new(op, source, target, class, operation, perm_set),
        ))
    }
}

impl<'a> SeObjectParser<'a> for TypeState<'a> {
    fn parse(input: &'a str) -> IResult<&'a str, Self> {
        let (input, op) = alt((tag("permissive"), tag("enforce")))(input)?;

        let (input, _) = space1(input)?;
        let (input, stype) = parse_seobj_no_star(input)?;

        Ok((input, TypeState::new(op, stype)))
    }
}

impl<'a> SeObjectParser<'a> for Type<'a> {
    fn parse(input: &'a str) -> IResult<&'a str, Self> {
        let (input, _) = tag("type")(input)?;
        let (input, _) = space1(input)?;
        let (input, name) = parse_single_word(input)?;

        if input.is_empty() {
            return Ok((input, Type::new(name, vec!["domain"]))); // default to domain
        }

        let (input, _) = space1(input)?;
        let (input, attrs) = parse_seobj_no_star(input)?;

        Ok((input, Type::new(name, attrs)))
    }
}

impl<'a> SeObjectParser<'a> for TypeAttr<'a> {
    fn parse(input: &'a str) -> IResult<&'a str, Self> {
        let (input, _) = alt((tag("typeattribute"), tag("attradd")))(input)?;
        let (input, _) = space1(input)?;
        let (input, stype) = parse_seobj_no_star(input)?;
        let (input, _) = space1(input)?;
        let (input, attr) = parse_seobj_no_star(input)?;

        Ok((input, TypeAttr::new(stype, attr)))
    }
}

impl<'a> SeObjectParser<'a> for Attr<'a> {
    fn parse(input: &'a str) -> IResult<&'a str, Self> {
        let (input, _) = tag("attribute")(input)?;
        let (input, _) = space1(input)?;
        let (input, attr) = parse_single_word(input)?;

        Ok((input, Attr::new(attr)))
    }
}

impl<'a> SeObjectParser<'a> for TypeTransition<'a> {
    fn parse(input: &'a str) -> IResult<&'a str, Self> {
        let (input, _) = alt((tag("type_transition"), tag("name_transition")))(input)?;
        let (input, _) = space1(input)?;
        let (input, source) = parse_single_word(input)?;
        let (input, _) = space1(input)?;
        let (input, target) = parse_single_word(input)?;
        let (input, _) = space1(input)?;
        let (input, class) = parse_single_word(input)?;
        let (input, _) = space1(input)?;
        let (input, default) = parse_single_word(input)?;

        if input.is_empty() {
            return Ok((
                input,
                TypeTransition::new(source, target, class, default, None),
            ));
        }

        let (input, _) = space1(input)?;
        let (input, object) = parse_single_word(input)?;

        Ok((
            input,
            TypeTransition::new(source, target, class, default, Some(object)),
        ))
    }
}

impl<'a> SeObjectParser<'a> for TypeChange<'a> {
    fn parse(input: &'a str) -> IResult<&'a str, Self> {
        let (input, op) = alt((tag("type_change"), tag("type_member")))(input)?;
        let (input, _) = space1(input)?;
        let (input, source) = parse_single_word(input)?;
        let (input, _) = space1(input)?;
        let (input, target) = parse_single_word(input)?;
        let (input, _) = space1(input)?;
        let (input, class) = parse_single_word(input)?;
        let (input, _) = space1(input)?;
        let (input, default) = parse_single_word(input)?;

        Ok((input, TypeChange::new(op, source, target, class, default)))
    }
}

impl<'a> SeObjectParser<'a> for GenFsCon<'a> {
    fn parse(input: &'a str) -> IResult<&'a str, Self>
    where
        Self: Sized,
    {
        let (input, _) = tag("genfscon")(input)?;
        let (input, _) = space1(input)?;
        let (input, fs) = parse_single_word(input)?;
        let (input, _) = space1(input)?;
        let (input, path) = parse_single_word(input)?;
        let (input, _) = space1(input)?;
        let (input, context) = parse_single_word(input)?;
        Ok((input, GenFsCon::new(fs, path, context)))
    }
}

impl<'a> PolicyStatement<'a> {
    fn parse(input: &'a str) -> IResult<&'a str, Self> {
        let (input, _) = space0(input)?;
        let (input, statement) = alt((
            map(NormalPerm::parse, PolicyStatement::NormalPerm),
            map(XPerm::parse, PolicyStatement::XPerm),
            map(TypeState::parse, PolicyStatement::TypeState),
            map(Type::parse, PolicyStatement::Type),
            map(TypeAttr::parse, PolicyStatement::TypeAttr),
            map(Attr::parse, PolicyStatement::Attr),
            map(TypeTransition::parse, PolicyStatement::TypeTransition),
            map(TypeChange::parse, PolicyStatement::TypeChange),
            map(GenFsCon::parse, PolicyStatement::GenFsCon),
        ))(input)?;
        let (input, _) = space0(input)?;
        let (input, _) = take_while(|c| c == ';')(input)?;
        let (input, _) = space0(input)?;
        Ok((input, statement))
    }
}

fn parse_sepolicy<'a, 'b>(input: &'b str, strict: bool) -> Result<Vec<PolicyStatement<'a>>>
where
    'b: 'a,
{
    let mut statements = vec![];

    for line in input.split(['\n', ';']) {
        if line.trim().is_empty() {
            continue;
        }
        if let Ok((_, statement)) = PolicyStatement::parse(line.trim()) {
            statements.push(statement);
        } else if strict {
            bail!("Failed to parse policy statement: {}", line)
        }
    }
    Ok(statements)
}

const SEPOLICY_MAX_LEN: usize = 128;

const CMD_NORMAL_PERM: u32 = 1;
const CMD_XPERM: u32 = 2;
const CMD_TYPE_STATE: u32 = 3;
const CMD_TYPE: u32 = 4;
const CMD_TYPE_ATTR: u32 = 5;
const CMD_ATTR: u32 = 6;
const CMD_TYPE_TRANSITION: u32 = 7;
const CMD_TYPE_CHANGE: u32 = 8;
const CMD_GENFSCON: u32 = 9;

#[derive(Debug, Default)]
enum PolicyObject {
    All, // for "*", stand for all objects, and is NULL in ffi
    One([u8; SEPOLICY_MAX_LEN]),
    #[default]
    None,
}

impl TryFrom<&str> for PolicyObject {
    type Error = anyhow::Error;
    fn try_from(s: &str) -> Result<Self> {
        anyhow::ensure!(s.len() <= SEPOLICY_MAX_LEN, "policy object too long");
        if s == "*" {
            return Ok(PolicyObject::All);
        }
        let mut buf = [0u8; SEPOLICY_MAX_LEN];
        buf[..s.len()].copy_from_slice(s.as_bytes());
        Ok(PolicyObject::One(buf))
    }
}

/// atomic statement, such as: allow domain1 domain2:file1 read;
/// normal statement would be expand to atomic statement, for example:
/// allow domain1 domain2:file1 { read write }; would be expand to two atomic statement
/// allow domain1 domain2:file1 read;allow domain1 domain2:file1 write;
#[allow(clippy::too_many_arguments)]
#[derive(Debug, new)]
struct AtomicStatement {
    cmd: u32,
    subcmd: u32,
    sepol1: PolicyObject,
    sepol2: PolicyObject,
    sepol3: PolicyObject,
    sepol4: PolicyObject,
    sepol5: PolicyObject,
    sepol6: PolicyObject,
    sepol7: PolicyObject,
}

impl<'a> TryFrom<&'a NormalPerm<'a>> for Vec<AtomicStatement> {
    type Error = anyhow::Error;
    fn try_from(perm: &'a NormalPerm<'a>) -> Result<Self> {
        let mut result = vec![];
        let subcmd = match perm.op {
            "allow" => 1,
            "deny" => 2,
            "auditallow" => 3,
            "dontaudit" => 4,
            _ => 0,
        };
        for &s in &perm.source {
            for &t in &perm.target {
                for &c in &perm.class {
                    for &p in &perm.perm {
                        result.push(AtomicStatement {
                            cmd: CMD_NORMAL_PERM,
                            subcmd,
                            sepol1: s.try_into()?,
                            sepol2: t.try_into()?,
                            sepol3: c.try_into()?,
                            sepol4: p.try_into()?,
                            sepol5: PolicyObject::None,
                            sepol6: PolicyObject::None,
                            sepol7: PolicyObject::None,
                        });
                    }
                }
            }
        }
        Ok(result)
    }
}

impl<'a> TryFrom<&'a XPerm<'a>> for Vec<AtomicStatement> {
    type Error = anyhow::Error;
    fn try_from(perm: &'a XPerm<'a>) -> Result<Self> {
        let mut result = vec![];
        let subcmd = match perm.op {
            "allowxperm" => 1,
            "auditallowxperm" => 2,
            "dontauditxperm" => 3,
            _ => 0,
        };
        for &s in &perm.source {
            for &t in &perm.target {
                for &c in &perm.class {
                    result.push(AtomicStatement {
                        cmd: CMD_XPERM,
                        subcmd,
                        sepol1: s.try_into()?,
                        sepol2: t.try_into()?,
                        sepol3: c.try_into()?,
                        sepol4: perm.operation.try_into()?,
                        sepol5: perm.perm_set.try_into()?,
                        sepol6: PolicyObject::None,
                        sepol7: PolicyObject::None,
                    });
                }
            }
        }
        Ok(result)
    }
}

impl<'a> TryFrom<&'a TypeState<'a>> for Vec<AtomicStatement> {
    type Error = anyhow::Error;
    fn try_from(perm: &'a TypeState<'a>) -> Result<Self> {
        let mut result = vec![];
        let subcmd = match perm.op {
            "permissive" => 1,
            "enforcing" => 2,
            _ => 0,
        };
        for &t in &perm.stype {
            result.push(AtomicStatement {
                cmd: CMD_TYPE_STATE,
                subcmd,
                sepol1: t.try_into()?,
                sepol2: PolicyObject::None,
                sepol3: PolicyObject::None,
                sepol4: PolicyObject::None,
                sepol5: PolicyObject::None,
                sepol6: PolicyObject::None,
                sepol7: PolicyObject::None,
            });
        }
        Ok(result)
    }
}

impl<'a> TryFrom<&'a Type<'a>> for Vec<AtomicStatement> {
    type Error = anyhow::Error;
    fn try_from(perm: &'a Type<'a>) -> Result<Self> {
        let mut result = vec![];
        for &attr in &perm.attrs {
            result.push(AtomicStatement {
                cmd: CMD_TYPE,
                subcmd: 0,
                sepol1: perm.name.try_into()?,
                sepol2: attr.try_into()?,
                sepol3: PolicyObject::None,
                sepol4: PolicyObject::None,
                sepol5: PolicyObject::None,
                sepol6: PolicyObject::None,
                sepol7: PolicyObject::None,
            });
        }
        Ok(result)
    }
}

impl<'a> TryFrom<&'a TypeAttr<'a>> for Vec<AtomicStatement> {
    type Error = anyhow::Error;
    fn try_from(perm: &'a TypeAttr<'a>) -> Result<Self> {
        let mut result = vec![];
        for &t in &perm.stype {
            for &attr in &perm.sattr {
                result.push(AtomicStatement {
                    cmd: CMD_TYPE_ATTR,
                    subcmd: 0,
                    sepol1: t.try_into()?,
                    sepol2: attr.try_into()?,
                    sepol3: PolicyObject::None,
                    sepol4: PolicyObject::None,
                    sepol5: PolicyObject::None,
                    sepol6: PolicyObject::None,
                    sepol7: PolicyObject::None,
                });
            }
        }
        Ok(result)
    }
}

impl<'a> TryFrom<&'a Attr<'a>> for Vec<AtomicStatement> {
    type Error = anyhow::Error;
    fn try_from(perm: &'a Attr<'a>) -> Result<Self> {
        let result = vec![AtomicStatement {
            cmd: CMD_ATTR,
            subcmd: 0,
            sepol1: perm.name.try_into()?,
            sepol2: PolicyObject::None,
            sepol3: PolicyObject::None,
            sepol4: PolicyObject::None,
            sepol5: PolicyObject::None,
            sepol6: PolicyObject::None,
            sepol7: PolicyObject::None,
        }];
        Ok(result)
    }
}

impl<'a> TryFrom<&'a TypeTransition<'a>> for Vec<AtomicStatement> {
    type Error = anyhow::Error;
    fn try_from(perm: &'a TypeTransition<'a>) -> Result<Self> {
        let mut result = vec![];
        let obj = match perm.object_name {
            Some(obj) => obj.try_into()?,
            None => PolicyObject::None,
        };
        result.push(AtomicStatement {
            cmd: CMD_TYPE_TRANSITION,
            subcmd: 0,
            sepol1: perm.source.try_into()?,
            sepol2: perm.target.try_into()?,
            sepol3: perm.class.try_into()?,
            sepol4: perm.default_type.try_into()?,
            sepol5: obj,
            sepol6: PolicyObject::None,
            sepol7: PolicyObject::None,
        });
        Ok(result)
    }
}

impl<'a> TryFrom<&'a TypeChange<'a>> for Vec<AtomicStatement> {
    type Error = anyhow::Error;
    fn try_from(perm: &'a TypeChange<'a>) -> Result<Self> {
        let mut result = vec![];
        let subcmd = match perm.op {
            "type_change" => 1,
            "type_member" => 2,
            _ => 0,
        };
        result.push(AtomicStatement {
            cmd: CMD_TYPE_CHANGE,
            subcmd,
            sepol1: perm.source.try_into()?,
            sepol2: perm.target.try_into()?,
            sepol3: perm.class.try_into()?,
            sepol4: perm.default_type.try_into()?,
            sepol5: PolicyObject::None,
            sepol6: PolicyObject::None,
            sepol7: PolicyObject::None,
        });
        Ok(result)
    }
}

impl<'a> TryFrom<&'a GenFsCon<'a>> for Vec<AtomicStatement> {
    type Error = anyhow::Error;
    fn try_from(perm: &'a GenFsCon<'a>) -> Result<Self> {
        let result = vec![AtomicStatement {
            cmd: CMD_GENFSCON,
            subcmd: 0,
            sepol1: perm.fs_name.try_into()?,
            sepol2: perm.partial_path.try_into()?,
            sepol3: perm.fs_context.try_into()?,
            sepol4: PolicyObject::None,
            sepol5: PolicyObject::None,
            sepol6: PolicyObject::None,
            sepol7: PolicyObject::None,
        }];
        Ok(result)
    }
}

impl<'a> TryFrom<&'a PolicyStatement<'a>> for Vec<AtomicStatement> {
    type Error = anyhow::Error;
    fn try_from(value: &'a PolicyStatement) -> Result<Self> {
        match value {
            PolicyStatement::NormalPerm(perm) => perm.try_into(),
            PolicyStatement::XPerm(perm) => perm.try_into(),
            PolicyStatement::TypeState(perm) => perm.try_into(),
            PolicyStatement::Type(perm) => perm.try_into(),
            PolicyStatement::TypeAttr(perm) => perm.try_into(),
            PolicyStatement::Attr(perm) => perm.try_into(),
            PolicyStatement::TypeTransition(perm) => perm.try_into(),
            PolicyStatement::TypeChange(perm) => perm.try_into(),
            PolicyStatement::GenFsCon(perm) => perm.try_into(),
        }
    }
}

////////////////////////////////////////////////////////////////
///  for C FFI to call kernel interface
///////////////////////////////////////////////////////////////

#[derive(Debug)]
#[repr(C)]
struct FfiPolicy {
    cmd: u32,
    subcmd: u32,
    sepol1: *const libc::c_char,
    sepol2: *const libc::c_char,
    sepol3: *const libc::c_char,
    sepol4: *const libc::c_char,
    sepol5: *const libc::c_char,
    sepol6: *const libc::c_char,
    sepol7: *const libc::c_char,
}

fn to_c_ptr(pol: &PolicyObject) -> *const libc::c_char {
    match pol {
        PolicyObject::None | PolicyObject::All => std::ptr::null(),
        PolicyObject::One(s) => s.as_ptr().cast::<libc::c_char>(),
    }
}

impl From<AtomicStatement> for FfiPolicy {
    fn from(policy: AtomicStatement) -> FfiPolicy {
        FfiPolicy {
            cmd: policy.cmd,
            subcmd: policy.subcmd,
            sepol1: to_c_ptr(&policy.sepol1),
            sepol2: to_c_ptr(&policy.sepol2),
            sepol3: to_c_ptr(&policy.sepol3),
            sepol4: to_c_ptr(&policy.sepol4),
            sepol5: to_c_ptr(&policy.sepol5),
            sepol6: to_c_ptr(&policy.sepol6),
            sepol7: to_c_ptr(&policy.sepol7),
        }
    }
}

#[cfg(any(target_os = "linux", target_os = "android"))]
fn apply_one_rule<'a>(statement: &'a PolicyStatement<'a>, strict: bool) -> Result<()> {
    let policies: Vec<AtomicStatement> = statement.try_into()?;

    for policy in policies {
        let mut result: u32 = 0;
        let cpolicy = FfiPolicy::from(policy);
        unsafe {
            #[allow(clippy::cast_possible_wrap)]
            libc::prctl(
                crate::ksu::KERNEL_SU_OPTION as i32, // supposed to overflow
                crate::ksu::CMD_SET_SEPOLICY,
                0,
                std::ptr::addr_of!(cpolicy).cast::<libc::c_void>(),
                std::ptr::addr_of_mut!(result).cast::<libc::c_void>(),
            );
        }

        if result != crate::ksu::KERNEL_SU_OPTION {
            log::warn!("apply rule: {:?} failed.", statement);
            if strict {
                return Err(anyhow::anyhow!("apply rule {:?} failed.", statement));
            }
        }
    }

    Ok(())
}

#[cfg(not(any(target_os = "linux", target_os = "android")))]
fn apply_one_rule<'a>(_statement: &'a PolicyStatement<'a>, _strict: bool) -> Result<()> {
    unimplemented!()
}

pub fn live_patch(policy: &str) -> Result<()> {
    let result = parse_sepolicy(policy.trim(), false)?;
    for statement in result {
        println!("{statement:?}");
        apply_one_rule(&statement, false)?;
    }
    Ok(())
}

pub fn apply_file<P: AsRef<Path>>(path: P) -> Result<()> {
    let input = std::fs::read_to_string(path)?;
    live_patch(&input)
}

pub fn check_rule(policy: &str) -> Result<()> {
    let path = Path::new(policy);
    let policy = if path.exists() {
        std::fs::read_to_string(path)?
    } else {
        policy.to_string()
    };
    let result = parse_sepolicy(policy.trim(), true)?;
    for statement in result {
        apply_one_rule(&statement, true)?;
    }
    Ok(())
}
