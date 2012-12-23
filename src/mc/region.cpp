/*
 * Copyright 2012 Moritz Hilscher
 *
 * This file is part of mapcrafter.
 *
 * mapcrafter is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mapcrafter is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mapcrafter.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mc/region.h"

#include <cstdlib>

namespace mapcrafter {
namespace mc {

RegionFile::RegionFile() {
}

RegionFile::RegionFile(const std::string& filename)
		: filename(filename) {
	regionpos = RegionPos::byFilename(filename);
}

RegionFile::~RegionFile() {
}

bool RegionFile::readHeaders(std::ifstream& file) {
	if (!file)
		return false;
	containing_chunks.clear();

	for (int i = 0; i < 1024; i++) {
		chunk_offsets[i] = 0;
		chunk_timestamps[i] = 0;
	}

	for (int x = 0; x < 32; x++) {
		for (int z = 0; z < 32; z++) {
			file.seekg(4 * (x + z * 32), std::ios::beg);
			int tmp;
			file.read((char*) &tmp, 4);
			if (tmp == 0)
				continue;
			int offset = be32toh(tmp << 8) * 4096;
			uint8_t sectors = ((uint8_t*) &tmp)[3];

			file.seekg(4096, std::ios::cur);
			int timestamp;
			file.read((char*) &timestamp, 4);
			timestamp = be32toh(timestamp);

			ChunkPos pos(x + regionpos.x * 32, z + regionpos.z * 32);
			containing_chunks.insert(pos);

			chunk_offsets[z * 32 + x] = offset;
			chunk_timestamps[z * 32 + x] = timestamp;
		}
	}
	return true;
}

bool RegionFile::loadAll() {
	std::ifstream file(filename.c_str(), std::ios_base::binary);
	if (!readHeaders(file))
		return false;
	file.seekg(0, std::ios::end);
	int filesize = file.tellg();
	file.seekg(0, std::ios::beg);

	regiondata.resize(filesize);
	file.read((char*) &regiondata[0], filesize);

	return true;
}

bool RegionFile::loadHeaders() {
	std::ifstream file(filename.c_str(), std::ios_base::binary);
	return readHeaders(file);
}

const std::string& RegionFile::getFilename() const {
	return filename;
}

const RegionPos& RegionFile::getPos() const {
	return regionpos;
}

const std::set<ChunkPos>& RegionFile::getContainingChunks() const {
	return containing_chunks;
}

bool RegionFile::hasChunk(const ChunkPos& chunk) const {
	return chunk_offsets[chunk.getLocalZ() * 32 + chunk.getLocalX()] != 0;
}

int RegionFile::getChunkTimestamp(const ChunkPos& chunk) const {
	return chunk_timestamps[chunk.getLocalZ() * 32 + chunk.getLocalX()];
}

int RegionFile::loadChunk(const ChunkPos& pos, Chunk& chunk) {
	if (!hasChunk(pos))
		return CHUNK_DOES_NOT_EXISTS;

	int offset = chunk_offsets[pos.getLocalZ() * 32 + pos.getLocalX()];

	int size = *((int*) &regiondata[offset]);
	uint8_t compression = regiondata[offset + 4];
	size = be32toh(size) - 1;

	try {
		if(!chunk.readNBT((char*) &regiondata[offset + 5], size))
			return CHUNK_INVALID;
	} catch (const nbt::NBTError& err) {
		std::cout << "Error: Unable to read chunk at " << pos.x << ":" << pos.z
		        << " : " << err.what() << std::endl;
		return CHUNK_NBT_ERROR;
	}
	return CHUNK_OK;
}

}
}
