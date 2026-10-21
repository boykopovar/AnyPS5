use std::collections::BTreeMap;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

use wgpu::naga;

struct Stage {
    module: wgpu::ShaderModule,
    reflection: naga::Module,
}

enum Resource {
    Buffer,
    Texture { dimension: wgpu::TextureViewDimension, format: wgpu::TextureFormat, storage: bool },
    Sampler { comparison: bool },
}

fn parse(path: &Path) -> Result<(String, naga::Module), String> {
    let source = std::fs::read_to_string(path).map_err(|error| format!("{}: {error}", path.display()))?.replace("aps5_storage_format", "rgba8unorm").replace("override aps5_", "const aps5_");
    let module = naga::front::wgsl::parse_str(&source).map_err(|error| format!("{}: {}", path.display(), error.emit_to_string(&source)))?;
    Ok((source, module))
}

fn load(device: &wgpu::Device, path: &Path) -> Result<Stage, String> {
    let (source, reflection) = parse(path)?;
    let module = device.create_shader_module(wgpu::ShaderModuleDescriptor {
        label: Some(&path.display().to_string()),
        source: wgpu::ShaderSource::Wgsl(source.into()),
    });
    Ok(Stage { module, reflection })
}

fn scalar_kind(inner: &naga::TypeInner) -> Option<(naga::ScalarKind, u32)> {
    match inner {
        naga::TypeInner::Scalar(scalar) => Some((scalar.kind, 1)),
        naga::TypeInner::Vector { size, scalar } => Some((scalar.kind, *size as u32)),
        _ => None,
    }
}

fn vertex_format(inner: &naga::TypeInner) -> Result<wgpu::VertexFormat, String> {
    use wgpu::VertexFormat as F;
    let (kind, count) = scalar_kind(inner).ok_or("a vertex input is not a scalar or a vector")?;
    Ok(match (kind, count) {
        (naga::ScalarKind::Float, 1) => F::Float32,
        (naga::ScalarKind::Float, 2) => F::Float32x2,
        (naga::ScalarKind::Float, 3) => F::Float32x3,
        (naga::ScalarKind::Float, 4) => F::Float32x4,
        (naga::ScalarKind::Uint, 1) => F::Uint32,
        (naga::ScalarKind::Uint, 2) => F::Uint32x2,
        (naga::ScalarKind::Uint, 3) => F::Uint32x3,
        (naga::ScalarKind::Uint, 4) => F::Uint32x4,
        (naga::ScalarKind::Sint, 1) => F::Sint32,
        (naga::ScalarKind::Sint, 2) => F::Sint32x2,
        (naga::ScalarKind::Sint, 3) => F::Sint32x3,
        (naga::ScalarKind::Sint, 4) => F::Sint32x4,
        _ => return Err("a vertex input has an unsupported type".into()),
    })
}

fn target_format(inner: &naga::TypeInner) -> Result<wgpu::TextureFormat, String> {
    match scalar_kind(inner) {
        Some((naga::ScalarKind::Float, _)) => Ok(wgpu::TextureFormat::Rgba8Unorm),
        Some((naga::ScalarKind::Uint, _)) => Ok(wgpu::TextureFormat::Rgba32Uint),
        Some((naga::ScalarKind::Sint, _)) => Ok(wgpu::TextureFormat::Rgba32Sint),
        _ => Err("a render target output has an unsupported type".into()),
    }
}

fn resources(stage: &Stage) -> Result<BTreeMap<(u32, u32), Resource>, String> {
    let mut result = BTreeMap::new();
    for (_, variable) in stage.reflection.global_variables.iter() {
        let Some(binding) = &variable.binding else { continue };
        let resource = match &stage.reflection.types[variable.ty].inner {
            naga::TypeInner::Array { .. } => Resource::Buffer,
            naga::TypeInner::Sampler { comparison } => Resource::Sampler { comparison: *comparison },
            naga::TypeInner::Image { dim, arrayed, class } => {
                let dimension = match (dim, arrayed) {
                    (naga::ImageDimension::D1, false) => wgpu::TextureViewDimension::D1,
                    (naga::ImageDimension::D2, false) => wgpu::TextureViewDimension::D2,
                    (naga::ImageDimension::D2, true) => wgpu::TextureViewDimension::D2Array,
                    (naga::ImageDimension::D3, false) => wgpu::TextureViewDimension::D3,
                    (naga::ImageDimension::Cube, false) => wgpu::TextureViewDimension::Cube,
                    (naga::ImageDimension::Cube, true) => wgpu::TextureViewDimension::CubeArray,
                    _ => return Err("an image has an unsupported dimension".into()),
                };
                let format = match class {
                    naga::ImageClass::Sampled { kind: naga::ScalarKind::Float, multi: false } => wgpu::TextureFormat::Rgba8Unorm,
                    naga::ImageClass::Sampled { kind: naga::ScalarKind::Uint, multi: false } => wgpu::TextureFormat::Rgba8Uint,
                    naga::ImageClass::Sampled { kind: naga::ScalarKind::Sint, multi: false } => wgpu::TextureFormat::Rgba8Sint,
                    naga::ImageClass::Storage { format: naga::StorageFormat::Rgba8Unorm, .. } => wgpu::TextureFormat::Rgba8Unorm,
                    naga::ImageClass::Depth { multi: false } => wgpu::TextureFormat::Depth32Float,
                    _ => return Err("an image has an unsupported class".into()),
                };
                Resource::Texture { dimension, format, storage: matches!(class, naga::ImageClass::Storage { .. }) }
            }
            _ => return Err("a binding has an unsupported type".into()),
        };
        result.insert((binding.group, binding.binding), resource);
    }
    Ok(result)
}

fn entry(stage: &Stage) -> Result<&naga::EntryPoint, String> {
    stage.reflection.entry_points.iter().find(|entry| entry.name == "main").ok_or_else(|| "the module has no main entry point".into())
}

fn bind_resource(device: &wgpu::Device, resource: &Resource) -> (Option<wgpu::Buffer>, Option<wgpu::TextureView>, Option<wgpu::Sampler>) {
    match resource {
        Resource::Buffer => {
            let buffer = device.create_buffer(&wgpu::BufferDescriptor { label: None, size: 4096, usage: wgpu::BufferUsages::STORAGE, mapped_at_creation: false });
            (Some(buffer), None, None)
        }
        Resource::Texture { dimension, format, storage } => {
            let (size, texture_dimension) = match dimension {
                wgpu::TextureViewDimension::D1 => (wgpu::Extent3d { width: 4, height: 1, depth_or_array_layers: 1 }, wgpu::TextureDimension::D1),
                wgpu::TextureViewDimension::D3 => (wgpu::Extent3d { width: 4, height: 4, depth_or_array_layers: 4 }, wgpu::TextureDimension::D3),
                wgpu::TextureViewDimension::Cube | wgpu::TextureViewDimension::CubeArray => (wgpu::Extent3d { width: 4, height: 4, depth_or_array_layers: 6 }, wgpu::TextureDimension::D2),
                wgpu::TextureViewDimension::D2Array => (wgpu::Extent3d { width: 4, height: 4, depth_or_array_layers: 2 }, wgpu::TextureDimension::D2),
                _ => (wgpu::Extent3d { width: 4, height: 4, depth_or_array_layers: 1 }, wgpu::TextureDimension::D2),
            };
            let texture = device.create_texture(&wgpu::TextureDescriptor {
                label: None,
                size,
                mip_level_count: 1,
                sample_count: 1,
                dimension: texture_dimension,
                format: *format,
                usage: if *storage { wgpu::TextureUsages::STORAGE_BINDING } else { wgpu::TextureUsages::TEXTURE_BINDING },
                view_formats: &[],
            });
            let view = texture.create_view(&wgpu::TextureViewDescriptor { dimension: Some(*dimension), ..Default::default() });
            (None, Some(view), None)
        }
        Resource::Sampler { comparison } => {
            let compare = comparison.then_some(wgpu::CompareFunction::LessEqual);
            let sampler = device.create_sampler(&wgpu::SamplerDescriptor { mag_filter: wgpu::FilterMode::Linear, min_filter: wgpu::FilterMode::Linear, compare, ..Default::default() });
            (None, None, Some(sampler))
        }
    }
}

fn check(device: &wgpu::Device, queue: &wgpu::Queue, vertex_path: &Path, pixel_path: Option<&Path>) -> Result<(), String> {
    let scope = device.push_error_scope(wgpu::ErrorFilter::Validation);
    let vertex = load(device, vertex_path)?;
    let pixel = pixel_path.map(|path| load(device, path)).transpose()?;

    let mut layouts = Vec::new();
    let mut vertex_buffers = Vec::new();
    for argument in &entry(&vertex)?.function.arguments {
        let Some(naga::Binding::Location { location, .. }) = &argument.binding else { continue };
        let format = vertex_format(&vertex.reflection.types[argument.ty].inner)?;
        let size = format.size();
        let mut data = vec![0u8; size as usize * 3];
        if matches!(format, wgpu::VertexFormat::Float32x2 | wgpu::VertexFormat::Float32x3 | wgpu::VertexFormat::Float32x4) && *location == 0 {
            for (index, (x, y)) in [(-1.0f32, -1.0f32), (3.0, -1.0), (-1.0, 3.0)].into_iter().enumerate() {
                let offset = index * size as usize;
                data[offset..offset + 4].copy_from_slice(&x.to_le_bytes());
                data[offset + 4..offset + 8].copy_from_slice(&y.to_le_bytes());
            }
        }
        let buffer = device.create_buffer(&wgpu::BufferDescriptor { label: None, size: data.len() as u64, usage: wgpu::BufferUsages::VERTEX | wgpu::BufferUsages::COPY_DST, mapped_at_creation: false });
        queue.write_buffer(&buffer, 0, &data);
        vertex_buffers.push(buffer);
        layouts.push((size, [wgpu::VertexAttribute { format, offset: 0, shader_location: *location }]));
    }
    let buffer_layouts: Vec<_> = layouts.iter().map(|(stride, attributes)| Some(wgpu::VertexBufferLayout { array_stride: *stride, step_mode: wgpu::VertexStepMode::Vertex, attributes })).collect();

    let mut targets: Vec<Option<wgpu::ColorTargetState>> = Vec::new();
    if let Some(pixel) = &pixel {
        if let Some(result) = &entry(pixel)?.function.result {
            if let naga::TypeInner::Struct { members, .. } = &pixel.reflection.types[result.ty].inner {
                for member in members {
                    let Some(naga::Binding::Location { location, .. }) = &member.binding else { continue };
                    let format = target_format(&pixel.reflection.types[member.ty].inner)?;
                    if targets.len() <= *location as usize {
                        targets.resize(*location as usize + 1, None);
                    }
                    targets[*location as usize] = Some(wgpu::ColorTargetState { format, blend: None, write_mask: wgpu::ColorWrites::ALL });
                }
            }
        }
    }

    let depth_only = targets.iter().all(Option::is_none);
    let pipeline = device.create_render_pipeline(&wgpu::RenderPipelineDescriptor {
        label: None,
        layout: None,
        vertex: wgpu::VertexState { module: &vertex.module, entry_point: Some("main"), compilation_options: Default::default(), buffers: &buffer_layouts },
        primitive: wgpu::PrimitiveState::default(),
        depth_stencil: depth_only.then(|| wgpu::DepthStencilState { format: wgpu::TextureFormat::Depth32Float, depth_write_enabled: Some(true), depth_compare: Some(wgpu::CompareFunction::Always), stencil: Default::default(), bias: Default::default() }),
        multisample: wgpu::MultisampleState::default(),
        fragment: pixel.as_ref().map(|pixel| wgpu::FragmentState { module: &pixel.module, entry_point: Some("main"), compilation_options: Default::default(), targets: &targets }),
        multiview_mask: None,
        cache: None,
    });

    let mut all = resources(&vertex)?;
    if let Some(pixel) = &pixel {
        all.extend(resources(pixel)?);
    }
    let bind_groups = bind_groups(device, |group| pipeline.get_bind_group_layout(group), &all);

    let views: Vec<_> = targets
        .iter()
        .map(|target| {
            target.as_ref().map(|target| {
                device
                    .create_texture(&wgpu::TextureDescriptor {
                        label: None,
                        size: wgpu::Extent3d { width: 64, height: 64, depth_or_array_layers: 1 },
                        mip_level_count: 1,
                        sample_count: 1,
                        dimension: wgpu::TextureDimension::D2,
                        format: target.format,
                        usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
                        view_formats: &[],
                    })
                    .create_view(&Default::default())
            })
        })
        .collect();
    let attachments: Vec<_> = views
        .iter()
        .map(|view| {
            view.as_ref().map(|view| wgpu::RenderPassColorAttachment { view, depth_slice: None, resolve_target: None, ops: wgpu::Operations { load: wgpu::LoadOp::Clear(wgpu::Color::BLACK), store: wgpu::StoreOp::Store } })
        })
        .collect();
    let depth_view = depth_only.then(|| {
        device
            .create_texture(&wgpu::TextureDescriptor {
                label: None,
                size: wgpu::Extent3d { width: 64, height: 64, depth_or_array_layers: 1 },
                mip_level_count: 1,
                sample_count: 1,
                dimension: wgpu::TextureDimension::D2,
                format: wgpu::TextureFormat::Depth32Float,
                usage: wgpu::TextureUsages::RENDER_ATTACHMENT,
                view_formats: &[],
            })
            .create_view(&Default::default())
    });
    let depth_attachment = depth_view.as_ref().map(|view| wgpu::RenderPassDepthStencilAttachment { view, depth_ops: Some(wgpu::Operations { load: wgpu::LoadOp::Clear(1.0), store: wgpu::StoreOp::Store }), stencil_ops: None });
    let mut encoder = device.create_command_encoder(&Default::default());
    {
        let mut pass = encoder.begin_render_pass(&wgpu::RenderPassDescriptor { label: None, color_attachments: &attachments, depth_stencil_attachment: depth_attachment, timestamp_writes: None, occlusion_query_set: None, multiview_mask: None });
        pass.set_pipeline(&pipeline);
        for (index, group) in bind_groups.iter().enumerate() {
            pass.set_bind_group(index as u32, group, &[]);
        }
        for (index, buffer) in vertex_buffers.iter().enumerate() {
            pass.set_vertex_buffer(index as u32, buffer.slice(..));
        }
        pass.draw(0..3, 0..1);
    }
    queue.submit([encoder.finish()]);
    device.poll(wgpu::PollType::wait_indefinitely()).map_err(|error| error.to_string())?;
    match pollster::block_on(scope.pop()) {
        Some(error) => Err(error.to_string()),
        None => Ok(()),
    }
}

fn bind_groups(device: &wgpu::Device, layout: impl Fn(u32) -> wgpu::BindGroupLayout, all: &BTreeMap<(u32, u32), Resource>) -> Vec<wgpu::BindGroup> {
    let groups = all.keys().map(|(group, _)| *group + 1).max().unwrap_or(0);
    let owned: Vec<_> = all.iter().map(|((group, binding), resource)| (*group, *binding, bind_resource(device, resource))).collect();
    (0..groups)
        .map(|group| {
            let entries: Vec<_> = owned
                .iter()
                .filter(|(owner, _, _)| *owner == group)
                .map(|(_, binding, (buffer, view, sampler))| wgpu::BindGroupEntry {
                    binding: *binding,
                    resource: match (buffer, view, sampler) {
                        (Some(buffer), _, _) => buffer.as_entire_binding(),
                        (_, Some(view), _) => wgpu::BindingResource::TextureView(view),
                        (_, _, Some(sampler)) => wgpu::BindingResource::Sampler(sampler),
                        _ => unreachable!(),
                    },
                })
                .collect();
            device.create_bind_group(&wgpu::BindGroupDescriptor { label: None, layout: &layout(group), entries: &entries })
        })
        .collect()
}

fn check_compute(device: &wgpu::Device, queue: &wgpu::Queue, path: &Path) -> Result<(), String> {
    let scope = device.push_error_scope(wgpu::ErrorFilter::Validation);
    let compute = load(device, path)?;
    let pipeline = device.create_compute_pipeline(&wgpu::ComputePipelineDescriptor { label: None, layout: None, module: &compute.module, entry_point: Some("main"), compilation_options: Default::default(), cache: None });
    let groups = bind_groups(device, |group| pipeline.get_bind_group_layout(group), &resources(&compute)?);
    let mut encoder = device.create_command_encoder(&Default::default());
    {
        let mut pass = encoder.begin_compute_pass(&Default::default());
        pass.set_pipeline(&pipeline);
        for (index, group) in groups.iter().enumerate() {
            pass.set_bind_group(index as u32, group, &[]);
        }
        pass.dispatch_workgroups(1, 1, 1);
    }
    queue.submit([encoder.finish()]);
    device.poll(wgpu::PollType::wait_indefinitely()).map_err(|error| error.to_string())?;
    match pollster::block_on(scope.pop()) {
        Some(error) => Err(error.to_string()),
        None => Ok(()),
    }
}

fn main() -> ExitCode {
    let arguments: Vec<PathBuf> = std::env::args_os().skip(1).map(PathBuf::from).collect();
    if arguments.is_empty() {
        eprintln!("usage: anyps5-wgsl-check <capture prefix>...");
        return ExitCode::FAILURE;
    }
    let instance = wgpu::Instance::new(wgpu::InstanceDescriptor { backends: wgpu::Backends::PRIMARY, ..wgpu::InstanceDescriptor::new_without_display_handle() });
    let adapter = match pollster::block_on(instance.request_adapter(&wgpu::RequestAdapterOptions { power_preference: wgpu::PowerPreference::HighPerformance, ..Default::default() })) {
        Ok(adapter) => adapter,
        Err(error) => {
            eprintln!("no adapter: {error}");
            return ExitCode::FAILURE;
        }
    };
    let info = adapter.get_info();
    println!("adapter: {} ({:?})", info.name, info.backend);
    let (device, queue) = match pollster::block_on(adapter.request_device(&wgpu::DeviceDescriptor { required_limits: adapter.limits(), ..Default::default() })) {
        Ok(pair) => pair,
        Err(error) => {
            eprintln!("no device: {error}");
            return ExitCode::FAILURE;
        }
    };
    let mut failures = 0;
    for prefix in &arguments {
        let compute = PathBuf::from(format!("{}.cs.wgsl", prefix.display()));
        let vertex = PathBuf::from(format!("{}.vs.wgsl", prefix.display()));
        let pixel = PathBuf::from(format!("{}.ps.wgsl", prefix.display()));
        let pixel = pixel.exists().then_some(pixel);
        let stages = [&compute, &vertex].into_iter().chain(pixel.as_ref());
        if stages.filter(|path| path.exists()).any(|path| std::fs::read_to_string(path).is_ok_and(|source| source.contains("enable subgroups;"))) {
            println!("SKIP {}: Naga does not implement the subgroups extension", prefix.display());
            continue;
        }
        let result = if compute.exists() { check_compute(&device, &queue, &compute) } else { check(&device, &queue, &vertex, pixel.as_deref()) };
        match result {
            Ok(()) => println!("OK   {}", prefix.display()),
            Err(error) => {
                failures += 1;
                println!("FAIL {}: {error}", prefix.display());
            }
        }
    }
    if failures == 0 { ExitCode::SUCCESS } else { ExitCode::FAILURE }
}
