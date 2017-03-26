#include "voxel_map.h"
#include "core/os/os.h"

//----------------------------------------------------------------------------
// VoxelBlock
//----------------------------------------------------------------------------

MeshInstance * VoxelBlock::get_mesh_instance(const Node & root) {
	if (mesh_instance_path.is_empty())
		return NULL;
	Node * n = root.get_node(mesh_instance_path);
	if (n == NULL)
		return NULL;
	return n->cast_to<MeshInstance>();
}

StaticBody * VoxelBlock::get_physics_body(const Node & root) {
	if (mesh_instance_path.is_empty())
		return NULL;
	Node * n = root.get_node(body_path);
	if (n == NULL)
		return NULL;
	return n->cast_to<StaticBody>();
}

// Helper
VoxelBlock * VoxelBlock::create(Vector3i bpos, Ref<VoxelBuffer> buffer) {
	const int bs = VoxelBlock::SIZE;
	ERR_FAIL_COND_V(buffer.is_null(), NULL);
	ERR_FAIL_COND_V(buffer->get_size() != Vector3i(bs, bs, bs), NULL);

	VoxelBlock * block = memnew(VoxelBlock);
	block->pos = bpos;

	block->voxels = buffer;
	//block->map = &map;
	return block;
}

VoxelBlock::VoxelBlock(): voxels(NULL) {
}

//----------------------------------------------------------------------------
// VoxelMap
//----------------------------------------------------------------------------

VoxelMap::VoxelMap() : _last_accessed_block(NULL) {
	for (unsigned int i = 0; i < VoxelBuffer::MAX_CHANNELS; ++i) {
		_default_voxel[i] = 0;
	}
}

VoxelMap::~VoxelMap() {
	clear();
}

int VoxelMap::get_voxel(Vector3i pos, unsigned int c) {
	Vector3i bpos = voxel_to_block(pos);
	VoxelBlock * block = get_block(bpos);
	if (block == NULL) {
		return _default_voxel[c];
	}
	return block->voxels->get_voxel(pos - block_to_voxel(bpos), c);
}

void VoxelMap::set_voxel(int value, Vector3i pos, unsigned int c) {

	Vector3i bpos = voxel_to_block(pos);
	VoxelBlock * block = get_block(bpos);

	if (block == NULL) {

		Ref<VoxelBuffer> buffer(memnew(VoxelBuffer));
		buffer->create(VoxelBlock::SIZE, VoxelBlock::SIZE, VoxelBlock::SIZE);
		buffer->set_default_values(_default_voxel);

		block = VoxelBlock::create(bpos, buffer);

		set_block(bpos, block);
	}

	block->voxels->set_voxel(value, pos - block_to_voxel(bpos), c);
}

void VoxelMap::set_default_voxel(int value, unsigned int channel) {
	ERR_FAIL_INDEX(channel, VoxelBuffer::MAX_CHANNELS);
	_default_voxel[channel] = value;
}

int VoxelMap::get_default_voxel(unsigned int channel) {
	ERR_FAIL_INDEX_V(channel, VoxelBuffer::MAX_CHANNELS, 0);
	return _default_voxel[channel];
}

VoxelBlock * VoxelMap::get_block(Vector3i bpos) {
	if (_last_accessed_block && _last_accessed_block->pos == bpos) {
		return _last_accessed_block;
	}
	VoxelBlock ** p = _blocks.getptr(bpos);
	if (p) {
		_last_accessed_block = *p;
		return _last_accessed_block;
	}
	return NULL;
}

void VoxelMap::set_block(Vector3i bpos, VoxelBlock * block) {
	ERR_FAIL_COND(block == NULL);
	if (_last_accessed_block == NULL || _last_accessed_block->pos == bpos) {
		_last_accessed_block = block;
	}
	_blocks.set(bpos, block);
}

void VoxelMap::set_block_buffer(Vector3i bpos, Ref<VoxelBuffer> buffer) {
	ERR_FAIL_COND(buffer.is_null());
	VoxelBlock * block = get_block(bpos);
	if (block == NULL) {
		block = VoxelBlock::create(bpos, *buffer);
		set_block(bpos, block);
	}
	else {
		block->voxels = buffer;
	}
}

bool VoxelMap::has_block(Vector3i pos) const {
	return /*(_last_accessed_block != NULL && _last_accessed_block->pos == pos) ||*/ _blocks.has(pos);
}

Vector3i g_moore_neighboring_3d[26] = {
	Vector3i(-1,-1,-1),
	Vector3i(0,-1,-1),
	Vector3i(1,-1,-1),
	Vector3i(-1,-1,0),
	Vector3i(0,-1,0),
	Vector3i(1,-1,0),
	Vector3i(-1,-1,1),
	Vector3i(0,-1,1),
	Vector3i(1,-1,1),

	Vector3i(-1,0,-1),
	Vector3i(0,0,-1),
	Vector3i(1,0,-1),
	Vector3i(-1,0,0),
	//Vector3i(0,0,0),
	Vector3i(1,0,0),
	Vector3i(-1,0,1),
	Vector3i(0,0,1),
	Vector3i(1,0,1),

	Vector3i(-1,1,-1),
	Vector3i(0,1,-1),
	Vector3i(1,1,-1),
	Vector3i(-1,1,0),
	Vector3i(0,1,0),
	Vector3i(1,1,0),
	Vector3i(-1,1,1),
	Vector3i(0,1,1),
	Vector3i(1,1,1),
};

bool VoxelMap::is_block_surrounded(Vector3i pos) const {
	for (unsigned int i = 0; i < 26; ++i) {
		Vector3i bpos = pos + g_moore_neighboring_3d[i];
		if (!has_block(bpos)) {
			return false;
		}
	}
	return true;
}

void VoxelMap::get_buffer_copy(Vector3i min_pos, VoxelBuffer & dst_buffer, unsigned int channel) {
	ERR_FAIL_INDEX(channel, VoxelBuffer::MAX_CHANNELS);

	Vector3i max_pos = min_pos + dst_buffer.get_size();

	Vector3i min_block_pos = voxel_to_block(min_pos);
	Vector3i max_block_pos = voxel_to_block(max_pos - Vector3i(1,1,1)) + Vector3i(1,1,1);
	ERR_FAIL_COND((max_block_pos - min_block_pos) != Vector3(3, 3, 3));

	Vector3i bpos;
	for (bpos.z = min_block_pos.z; bpos.z < max_block_pos.z; ++bpos.z) {
		for (bpos.x = min_block_pos.x; bpos.x < max_block_pos.x; ++bpos.x) {
			for (bpos.y = min_block_pos.y; bpos.y < max_block_pos.y; ++bpos.y) {

				VoxelBlock * block = get_block(bpos);
				if (block) {

					VoxelBuffer & src_buffer = **block->voxels;
					Vector3i offset = block_to_voxel(bpos);
					// Note: copy_from takes care of clamping the area if it's on an edge
					dst_buffer.copy_from(src_buffer, min_pos - offset, max_pos - offset, offset - min_pos, channel);
				}
				else {
					Vector3i offset = block_to_voxel(bpos);
					dst_buffer.fill_area(
						_default_voxel[channel],
						offset - min_pos,
						offset - min_pos + Vector3i(VoxelBlock::SIZE,VoxelBlock::SIZE, VoxelBlock::SIZE)
					);
				}

			}
		}
	}
}

void VoxelMap::remove_blocks_not_in_area(Vector3i min, Vector3i max) {

	Vector3i::sort_min_max(min, max);

	Vector<Vector3i> to_remove;
	const Vector3i * key = NULL;

	while (key = _blocks.next(key)) {

		VoxelBlock * block_ref = _blocks.get(*key);
		ERR_FAIL_COND(block_ref == NULL); // Should never trigger

		if (block_ref->pos.is_contained_in(min, max)) {

			//if (_observer)
			//    _observer->block_removed(block);

			to_remove.push_back(*key);

			if (block_ref == _last_accessed_block)
				_last_accessed_block = NULL;
		}
	}

	for (unsigned int i = 0; i < to_remove.size(); ++i) {
		_blocks.erase(to_remove[i]);
	}
}

void VoxelMap::clear() {
	const Vector3i * key = NULL;
	while (key = _blocks.next(key)) {
		VoxelBlock * block_ref = _blocks.get(*key);
		if(block_ref == NULL) {
			OS::get_singleton()->printerr("Unexpected NULL in VoxelMap::clear()");
		}
		memdelete(block_ref);
	}
	_blocks.clear();
	_last_accessed_block = NULL;
}


void VoxelMap::_bind_methods() {

	ClassDB::bind_method(D_METHOD("get_voxel", "x", "y", "z", "c"), &VoxelMap::_get_voxel_binding, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("set_voxel", "value", "x", "y", "z", "c"), &VoxelMap::_set_voxel_binding, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_default_voxel", "channel"), &VoxelMap::get_default_voxel, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("set_default_voxel", "value", "channel"), &VoxelMap::set_default_voxel, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("has_block", "x", "y", "z"), &VoxelMap::_has_block_binding);
	ClassDB::bind_method(D_METHOD("get_buffer_copy", "min_pos", "out_buffer:VoxelBuffer", "channel"), &VoxelMap::_get_buffer_copy_binding, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("set_block_buffer", "block_pos", "buffer:VoxelBuffer"), &VoxelMap::_set_block_buffer_binding);
	ClassDB::bind_method(D_METHOD("voxel_to_block", "voxel_pos"), &VoxelMap::_voxel_to_block_binding);
	ClassDB::bind_method(D_METHOD("block_to_voxel", "block_pos"), &VoxelMap::_block_to_voxel_binding);
	ClassDB::bind_method(D_METHOD("get_block_size"), &VoxelMap::get_block_size);

	//ADD_PROPERTY(PropertyInfo(Variant::INT, "iterations"), _SCS("set_iterations"), _SCS("get_iterations"));

}


void VoxelMap::_get_buffer_copy_binding(Vector3 pos, Ref<VoxelBuffer> dst_buffer_ref, unsigned int channel) {
	ERR_FAIL_COND(dst_buffer_ref.is_null());
	get_buffer_copy(Vector3i(pos), **dst_buffer_ref, channel);
}

