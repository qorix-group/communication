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

//! This crate provides a Iox2 implementation of the COM API.
//! It is meant to be used in conjunction with the `com-api` crate.

use com_api_concept::Reloc;
use core::cmp::Ordering;
use core::fmt::Debug;
use core::future::Future;
use core::marker::PhantomData;
use iceoryx2_qnx8::prelude::*;
use std::collections::HashSet;
use std::collections::VecDeque;
use std::path::Path;
use std::sync::Arc;

use com_api_concept::{
    Builder, Consumer, ConsumerBuilder, ConsumerDescriptor, FindServiceSpecifier,
    InstanceSpecifier, Interface, Producer, ProducerBuilder, Result, Runtime, SampleContainer,
    ServiceDiscovery, Subscriber, Subscription,
};

mod samples;
use samples::*;

// TODO: Consider node as static one since we need to keep it everywhere...

pub struct Iox2Runtime {
    node: Arc<Node<ipc_threadsafe::Service>>,
}
#[derive(Clone)]
pub struct Iox2ProviderInfo {
    instance_specifier: InstanceSpecifier,
    node: Arc<Node<ipc_threadsafe::Service>>,
}

#[derive(Clone)]
pub struct Iox2ConsumerInfo {
    instance_specifier: InstanceSpecifier,
    node: Arc<Node<ipc_threadsafe::Service>>,
}

impl Runtime for Iox2Runtime {
    type ServiceDiscovery<I: Interface> = Iox2ConsumerDiscovery<I>;
    type Subscriber<T: Reloc + Send + Debug + 'static> = Iox2Subscribable<T>;
    type ProducerBuilder<I: Interface> = Iox2ProducerBuilder<I>;
    type Publisher<T: Reloc + Send + Debug + 'static> = Publisher<T>;
    type ProviderInfo = Iox2ProviderInfo;
    type ConsumerInfo = Iox2ConsumerInfo;

    fn find_service<I: Interface>(
        &self,
        instance_specifier: FindServiceSpecifier,
    ) -> Self::ServiceDiscovery<I> {
        Iox2ConsumerDiscovery::new(self, instance_specifier)
    }

    fn producer_builder<I: Interface>(
        &self,
        instance_specifier: InstanceSpecifier,
    ) -> Self::ProducerBuilder<I> {
        Iox2ProducerBuilder::new(self, instance_specifier)
    }
}

fn create_pub_sub_service_name(
    specifier: &InstanceSpecifier,
    identifier: &str,
) -> com_api_concept::Result<ServiceName> {
    let service_name = format!("{}/{}", specifier.as_ref(), identifier);
    let service_name_iceoryx = ServiceName::new(service_name.as_str()).map_err(|e| {
        println!("Failed to create service name: {e}");
        com_api_concept::Error::SubscribeFailed
    })?;
    Ok(service_name_iceoryx)
}

pub struct Iox2Subscribable<T: Debug + ZeroCopySend> {
    service_name: ServiceName,
    node: Arc<Node<ipc_threadsafe::Service>>,
    _phantom: PhantomData<T>,
}

impl<T: Reloc + Send + Debug + ZeroCopySend + 'static> Iox2Subscribable<T> {
    fn from_info(
        identifier: &str,
        instance_info: Iox2ConsumerInfo,
    ) -> com_api_concept::Result<Self> {
        let service_name =
            create_pub_sub_service_name(&instance_info.instance_specifier, identifier)?;

        Ok(Self {
            service_name,
            node: instance_info.node,
            _phantom: PhantomData,
        })
    }

    fn from_unsubscribe(
        service_name: ServiceName,
        node: Arc<Node<ipc_threadsafe::Service>>,
    ) -> Self {
        Self {
            service_name,
            node,
            _phantom: PhantomData,
        }
    }
}

impl<T: Reloc + Send + Debug + ZeroCopySend + 'static> Subscriber<T, Iox2Runtime>
    for Iox2Subscribable<T>
{
    type Subscription = Iox2Subscriber<T>;

    fn new(identifier: &str, instance_info: Iox2ConsumerInfo) -> com_api_concept::Result<Self> {
        Self::from_info(identifier, instance_info)
    }

    fn subscribe(&self, _max_num_samples: usize) -> com_api_concept::Result<Self::Subscription> {
        println!(
            "Creating subscriber for service name: {}",
            self.service_name
        );
        let service = self
            .node
            .service_builder(&self.service_name)
            .publish_subscribe::<T>()
            .open()
            .map_err(|e| {
                println!("Failed to create service: {e}");
                com_api_concept::Error::SubscribeFailed
            })?;

        println!("Subscribing to service: {}", self.service_name.as_str());
        match service.subscriber_builder().create() {
            Err(e) => {
                println!("Failed to create subscriber: {e}");
                Err(com_api_concept::Error::SubscribeFailed)
            }
            Ok(subscriber) => Ok(Self::Subscription::new(
                Arc::clone(&self.node),
                subscriber,
                self.service_name.clone(),
            )),
        }
    }
}

pub struct Iox2Subscriber<T: Debug + ZeroCopySend + 'static>
where
    T: Reloc + Send + Debug + ZeroCopySend,
{
    data: VecDeque<T>,
    subscriber: iceoryx2_qnx8::port::subscriber::Subscriber<ipc_threadsafe::Service, T, ()>,
    node: Arc<Node<ipc_threadsafe::Service>>,
    service_name: ServiceName,
}

impl<T> Iox2Subscriber<T>
where
    T: Reloc + Send + Debug + ZeroCopySend,
{
    pub fn new(
        node: Arc<Node<ipc_threadsafe::Service>>,
        subscriber: iceoryx2_qnx8::port::subscriber::Subscriber<ipc_threadsafe::Service, T, ()>,
        service_name: ServiceName,
    ) -> Self {
        Self {
            data: Default::default(),
            subscriber,
            node,
            service_name,
        }
    }

    pub fn add_data(&mut self, data: T) {
        self.data.push_front(data);
    }
}

impl<T> Subscription<T, Iox2Runtime> for Iox2Subscriber<T>
where
    T: Reloc + Send + Debug + ZeroCopySend,
{
    type Subscriber = Iox2Subscribable<T>;
    type Sample<'a>
        = Iox2Sample<'a, T>
    where
        T: 'a;

    fn unsubscribe(self) -> Self::Subscriber {
        Self::Subscriber::from_unsubscribe(self.service_name, self.node)
    }

    fn try_receive<'a>(
        &'a self,
        scratch: &'_ mut SampleContainer<Self::Sample<'a>>,
        _max_samples: usize,
    ) -> com_api_concept::Result<usize> {
        let _result = self.subscriber.receive();
        match _result {
            Ok(option) => match option {
                Some(sample) => {
                    let _res = scratch.push_back(Iox2Sample::new(sample));
                    return Ok(1);
                }
                None => return Ok(0),
            },
            Err(_e) => Err(com_api_concept::Error::Fail),
        }
    }

    #[allow(clippy::manual_async_fn)]
    fn receive<'a>(
        &'a self,
        _scratch: &'_ mut SampleContainer<Self::Sample<'a>>,
        _new_samples: usize,
        _max_samples: usize,
    ) -> impl Future<Output = com_api_concept::Result<usize>> + Send {
        async { todo!() }
    }
}

pub struct Publisher<T: Debug + ZeroCopySend + 'static> {
    publisher: iceoryx2_qnx8::port::publisher::Publisher<ipc_threadsafe::Service, T, ()>,
    _phantom: PhantomData<T>,
}

impl<T: Debug + ZeroCopySend> Publisher<T> where T: Reloc + Send {}

impl<T> com_api_concept::Publisher<T, Iox2Runtime> for Publisher<T>
where
    T: Reloc + Send + Debug,
{
    type SampleMaybeUninit<'a>
        = Iox2SampleMaybeUninit<'a, T>
    where
        Self: 'a;

    fn new(identifier: &str, instance_info: Iox2ProviderInfo) -> com_api_concept::Result<Self> {
        let service_name =
            create_pub_sub_service_name(&instance_info.instance_specifier, identifier)?;

        println!("Creating publisher for service name: {}", service_name);
        let service_result = instance_info
            .node
            .service_builder(&service_name)
            .publish_subscribe::<T>()
            .create();

        match service_result {
            Err(e) => {
                println!("Failed to create publisher: {e}");
                return Err(com_api_concept::Error::Fail);
            }
            Ok(service) => match service.publisher_builder().create() {
                Err(e) => {
                    println!("Failed to create publisher: {e}");
                    return Err(com_api_concept::Error::Fail);
                }
                Ok(publisher) => {
                    return Ok(Self {
                        publisher: publisher,
                        _phantom: PhantomData,
                    });
                }
            },
        }
    }

    fn allocate<'a>(&'a self) -> com_api_concept::Result<Self::SampleMaybeUninit<'a>> {
        match self.publisher.loan_uninit() {
            Err(_e) => return Err(com_api_concept::Error::AllocateFailed),
            Ok(sample) => {
                return Ok(Iox2SampleMaybeUninit::new(sample));
            }
        }
    }
}

pub struct Iox2ConsumerDiscovery<I> {
    instance_specifier: FindServiceSpecifier,
    node: Arc<Node<ipc_threadsafe::Service>>,
    _phantom: PhantomData<I>,
}

impl<I> Iox2ConsumerDiscovery<I> {
    fn new(runtime: &Iox2Runtime, instance_specifier: FindServiceSpecifier) -> Self {
        Self {
            instance_specifier: instance_specifier,
            node: Arc::clone(&runtime.node),
            _phantom: PhantomData,
        }
    }
}

impl<I: Interface> ServiceDiscovery<I, Iox2Runtime> for Iox2ConsumerDiscovery<I>
where
    Iox2ConsumerBuilder<I>: ConsumerBuilder<I, Iox2Runtime>,
{
    type ConsumerBuilder = Iox2ConsumerBuilder<I>;
    type ServiceEnumerator = Vec<Iox2ConsumerBuilder<I>>;

    fn get_available_instances(&self) -> com_api_concept::Result<Self::ServiceEnumerator> {
        // TODO: This stays here but will be handled correctly in next PR
        let mut result: Vec<Iox2ConsumerBuilder<I>> = Vec::new();
        let mut result_temp: HashSet<String> = HashSet::new();

        let _ = ipc::Service::list(Config::global_config(), |service| {
            let mut service_name = service.static_details.name().to_string();
            let pos = service_name.rfind('/').unwrap_or(0);
            if pos > 0 {
                service_name = service_name.split_at(pos).0.to_string();
            }
            match self.instance_specifier {
                FindServiceSpecifier::Specific(ref s) => {
                    match service_name.cmp(&s.as_ref().to_string()) {
                        Ordering::Equal => {
                            result_temp.insert(service_name);
                            CallbackProgression::Stop
                        }
                        _ => CallbackProgression::Continue,
                    }
                }
                FindServiceSpecifier::Any => {
                    result_temp.insert(service_name);
                    CallbackProgression::Continue
                }
            }
        });

        result_temp.iter().for_each(|service_name| {
            result.push(Iox2ConsumerBuilder {
                instance_specifier: InstanceSpecifier::new(service_name).unwrap(),
                _phantom: PhantomData,
                node: Arc::clone(&self.node),
            });
        });

        Ok(result)
    }

    #[allow(clippy::manual_async_fn)]
    fn get_available_instances_async(
        &self,
    ) -> impl Future<Output = com_api_concept::Result<Self::ServiceEnumerator>> {
        async { todo!() }
    }
}

pub struct Iox2ProducerBuilder<I: Interface> {
    instance_specifier: InstanceSpecifier,
    node: Arc<Node<ipc_threadsafe::Service>>,
    _phantom: PhantomData<I>,
}

impl<I: Interface> Iox2ProducerBuilder<I> {
    fn new(runtime: &Iox2Runtime, instance_specifier: InstanceSpecifier) -> Self {
        Self {
            instance_specifier,
            node: Arc::clone(&runtime.node),
            _phantom: PhantomData,
        }
    }
}

impl<I: Interface> ProducerBuilder<I, Iox2Runtime> for Iox2ProducerBuilder<I> {}

impl<I: Interface> Builder<I::Producer<Iox2Runtime>> for Iox2ProducerBuilder<I> {
    fn build(self) -> Result<I::Producer<Iox2Runtime>> {
        let instance_info = Iox2ProviderInfo {
            instance_specifier: self.instance_specifier,
            node: self.node,
        };
        return I::Producer::new(instance_info);
    }
}

pub struct Iox2ConsumerDescriptor<I: Interface> {
    _phantom: PhantomData<I>,
}

pub struct Iox2ConsumerBuilder<I: Interface> {
    instance_specifier: InstanceSpecifier,
    node: Arc<Node<ipc_threadsafe::Service>>,
    _phantom: PhantomData<I>,
}

impl<I: Interface> ConsumerDescriptor<Iox2Runtime> for Iox2ConsumerBuilder<I> {
    fn get_instance_identifier(&self) -> &InstanceSpecifier {
        println!(
            "Getting instance identifier: {}",
            self.instance_specifier.as_ref().to_string()
        );
        &self.instance_specifier
    }
}

impl<I: Interface> ConsumerBuilder<I, Iox2Runtime> for Iox2ConsumerBuilder<I> {}

impl<I: Interface> Builder<I::Consumer<Iox2Runtime>> for Iox2ConsumerBuilder<I> {
    fn build(self) -> com_api_concept::Result<I::Consumer<Iox2Runtime>> {
        let instance_info = Iox2ConsumerInfo {
            instance_specifier: self.instance_specifier,
            node: self.node,
        };

        Ok(I::Consumer::new(instance_info))
    }
}

pub struct Iox2RuntimeBuilder {}

impl Iox2RuntimeBuilder {
    pub fn new() -> Self {
        Self {}
    }
}

impl Builder<Iox2Runtime> for Iox2RuntimeBuilder {
    fn build(self) -> com_api_concept::Result<Iox2Runtime> {
        // TODO Add config for node building here
        match NodeBuilder::new().create::<ipc_threadsafe::Service>() {
            Ok(node) => Ok(Iox2Runtime {
                node: Arc::new(node),
            }),
            Err(e) => {
                println!("Failed to create Iox2 node with {}", e);
                Err(com_api_concept::Error::Fail)
            }
        }
    }
}

impl com_api_concept::RuntimeBuilder<Iox2Runtime> for Iox2RuntimeBuilder {
    fn load_config(&mut self, _config: &Path) -> &mut Self {
        todo!("Loading configuration from file is not yet implemented")
    }
}
