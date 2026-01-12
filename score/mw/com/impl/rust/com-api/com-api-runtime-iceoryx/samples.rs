/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

use com_api_concept::Reloc;
use core::cmp::Ordering;
use core::fmt::Debug;
use core::marker::PhantomData;
use core::mem::MaybeUninit;
use core::ops::{Deref, DerefMut};
use iceoryx2_qnx8::prelude::*;
use std::sync::atomic::AtomicUsize;

static ID_COUNTER: AtomicUsize = AtomicUsize::new(0);

#[derive(Debug)]
pub struct Iox2Sample<'a, T>
where
    T: Send + Reloc + Debug + ZeroCopySend,
{
    id: usize,
    data: iceoryx2_qnx8::sample::Sample<ipc_threadsafe::Service, T, ()>,
    _phantom: PhantomData<&'a T>,
}

unsafe impl<'a, T> Send for Iox2Sample<'a, T> where T: Send + Reloc + Debug + ZeroCopySend {}

impl<'a, T> Iox2Sample<'a, T>
where
    T: Send + Reloc + Debug + ZeroCopySend,
{
    pub fn new(data: iceoryx2_qnx8::sample::Sample<ipc_threadsafe::Service, T, ()>) -> Self {
        Self {
            id: ID_COUNTER.fetch_add(1, std::sync::atomic::Ordering::Relaxed),
            data,
            _phantom: PhantomData,
        }
    }
}

impl<'a, T> PartialEq for Iox2Sample<'a, T>
where
    T: Send + Reloc + Debug + ZeroCopySend,
{
    fn eq(&self, other: &Self) -> bool {
        self.id == other.id
    }
}

impl<'a, T> Eq for Iox2Sample<'a, T> where T: Send + Reloc + Debug + ZeroCopySend {}

impl<'a, T> PartialOrd for Iox2Sample<'a, T>
where
    T: Send + Reloc + Debug + ZeroCopySend,
{
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl<'a, T> Ord for Iox2Sample<'a, T>
where
    T: Send + Reloc + Debug + ZeroCopySend,
{
    fn cmp(&self, other: &Self) -> Ordering {
        self.id.cmp(&other.id)
    }
}

impl<'a, T> Deref for Iox2Sample<'a, T>
where
    T: Reloc + Send + Debug + ZeroCopySend,
{
    type Target = T;

    fn deref(&self) -> &Self::Target {
        &self.data.payload()
    }
}

impl<'a, T> com_api_concept::Sample<T> for Iox2Sample<'a, T> where
    T: Send + Reloc + Debug + ZeroCopySend
{
}

#[derive(Debug)]
pub struct Iox2SampleMut<'a, T>
where
    T: Reloc + Debug + ZeroCopySend,
{
    data: iceoryx2_qnx8::sample_mut::SampleMut<ipc_threadsafe::Service, T, ()>,
    _phantom: PhantomData<&'a T>,
}

impl<'a, T> com_api_concept::SampleMut<T> for Iox2SampleMut<'a, T>
where
    T: Reloc + Send + Debug + ZeroCopySend,
{
    type Sample = Iox2Sample<'a, T>;

    fn into_sample(self) -> Self::Sample {
        unimplemented!("Clarification ongoing if API required") // If this is really required we need to store in Sample enum (Sample, SampleMut) and implement correct accessors...
    }

    fn send(self) -> com_api_concept::Result<()> {
        let return_value = self.data.send();
        match return_value {
            Err(e) => {
                println!("Failed to send sample: {e}");
                Err(com_api_concept::Error::Fail)
            }
            Ok(_) => Ok(()),
        }
    }
}

impl<'a, T> Deref for Iox2SampleMut<'a, T>
where
    T: Reloc + Debug + ZeroCopySend,
{
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.data.payload()
    }
}

impl<'a, T> DerefMut for Iox2SampleMut<'a, T>
where
    T: Reloc + Debug + ZeroCopySend,
{
    fn deref_mut(&mut self) -> &mut Self::Target {
        self.data.payload_mut()
    }
}

pub struct Iox2SampleMaybeUninit<'a, T>
where
    T: Reloc + Send + ZeroCopySend,
{
    data: iceoryx2_qnx8::sample_mut_uninit::SampleMutUninit<
        ipc_threadsafe::Service,
        MaybeUninit<T>,
        (),
    >,
    _phantom: PhantomData<&'a T>,
}

impl<'a, T> Iox2SampleMaybeUninit<'a, T>
where
    T: Reloc + Send + ZeroCopySend,
{
    pub fn new(
        data: iceoryx2_qnx8::sample_mut_uninit::SampleMutUninit<
            ipc_threadsafe::Service,
            MaybeUninit<T>,
            (),
        >,
    ) -> Self {
        Self {
            data,
            _phantom: PhantomData,
        }
    }
}

impl<'a, T> Debug for Iox2SampleMaybeUninit<'a, T>
where
    T: Reloc + Send + ZeroCopySend,
{
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "SampleMaybeUninit")
    }
}

impl<'a, T> com_api_concept::SampleMaybeUninit<T> for Iox2SampleMaybeUninit<'a, T>
where
    T: Reloc + Send + Debug + ZeroCopySend,
{
    type SampleMut = Iox2SampleMut<'a, T>;

    fn write(self, val: T) -> Iox2SampleMut<'a, T> {
        Self::SampleMut {
            data: self.data.write_payload(val),
            _phantom: PhantomData,
        }
    }

    unsafe fn assume_init(self) -> Iox2SampleMut<'a, T> {
        Self::SampleMut {
            data: unsafe { self.data.assume_init() },
            _phantom: PhantomData,
        }
    }
}

impl<'a, T> AsMut<core::mem::MaybeUninit<T>> for Iox2SampleMaybeUninit<'a, T>
where
    T: Reloc + Send + Debug,
{
    fn as_mut(&mut self) -> &mut core::mem::MaybeUninit<T> {
        self.data.payload_mut()
    }
}
