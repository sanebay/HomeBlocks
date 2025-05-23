
/*********************************************************************************
 * Modifications Copyright 2017-2019 eBay Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *    https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed
 * under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations under the License.
 *
 *********************************************************************************/
#pragma once
#include "index.hpp"
#include "sisl/utility/enum.hpp"
#include <homeblks/volume_mgr.hpp>
#include <homestore/homestore.hpp>
#include <homestore/index/index_table.hpp>
#include <homestore/replication/repl_dev.h>
#include <homeblks/common.hpp>

namespace homeblocks {
class VolumeIndexKey;
class VolumeIndexValue;
using VolumeIndexTable = homestore::IndexTable< VolumeIndexKey, VolumeIndexValue >;
using VolIdxTablePtr = shared< VolumeIndexTable >;

using ReplDevPtr = shared< homestore::ReplDev >;
using index_cfg_t = homestore::BtreeConfig;

ENUM(vol_state, uint32_t,
     INIT,       // initialized, but not ready online yet;
     ONLINE,     // online and ready to be used;
     OFFLINE,    // offline and not ready to be used;
     DESTROYING, // being destroyed, this state will be used for vol-destroy crash recovery;
     DESTROYED,  // fully destroyed, currently not used,
                 // for future use of lazy-destroy, e.g. set destroyed and move forward, let the volume be destroyed in
                 // background;
     READONLY    // in read only mode;
);

class Volume {

public:
    inline static auto const VOL_META_NAME = std::string("Volume2"); // different from old releae;
private:
    static constexpr uint64_t VOL_SB_MAGIC = 0xc01fadeb; // different from old release;
    static constexpr uint64_t VOL_SB_VER = 0x3;          // bump one from old release
    static constexpr uint64_t VOL_NAME_SIZE = 100;

    struct vol_sb_t {
        uint64_t magic;
        uint32_t version;
        uint32_t num_streams{0}; // number of streams in the volume; only used in HDD case;
        uint32_t page_size;
        uint64_t size; // privisioned size in bytes of volume;
        volume_id_t id;
        char name[VOL_NAME_SIZE];
        vol_state state{vol_state::INIT};

        void init(uint32_t page_sz, uint64_t sz_bytes, volume_id_t vid, std::string const& name_str) {
            magic = VOL_SB_MAGIC;
            version = VOL_SB_VER;
            page_size = page_sz;
            size = sz_bytes;
            id = vid;
            // name will be truncated if input name is longer than VOL_NAME_SIZE;
            std::strncpy((char*)name, name_str.c_str(), VOL_NAME_SIZE - 1);
            name[VOL_NAME_SIZE - 1] = '\0';
        }
    };

public:
    explicit Volume(VolumeInfo&& info) : sb_{VOL_META_NAME} {
        vol_info_ = std::make_shared< VolumeInfo >(info.id, info.size_bytes, info.page_size, info.name);
    }
    explicit Volume(sisl::byte_view const& buf, void* cookie);
    Volume(Volume const& volume) = delete;
    Volume(Volume&& volume) = default;
    Volume& operator=(Volume const& volume) = delete;
    Volume& operator=(Volume&& volume) = default;

    virtual ~Volume() = default;

    // static APIs exposed to HomeBlks Implementation Layer;
    static VolumePtr make_volume(sisl::byte_view const& buf, void* cookie) {
        auto vol = std::make_shared< Volume >(buf, cookie);
        auto ret = vol->init(true /*is_recovery*/);
        return ret ? vol : nullptr;
    }

    static VolumePtr make_volume(VolumeInfo&& info) {
        auto vol = std::make_shared< Volume >(std::move(info));
        auto ret = vol->init();
        // in failure case, volume shared ptr will be destroyed automatically;
        return ret ? vol : nullptr;
    }

    VolIdxTablePtr indx_table() const { return indx_tbl_; }
    volume_id_t id() const { return vol_info_->id; };
    std::string id_str() const { return boost::uuids::to_string(vol_info_->id); };
    ReplDevPtr rd() const { return rd_; }

    VolumeInfoPtr info() const { return vol_info_; }

    //
    // Initialize index table for this volume and saves the index handle in the volume object;
    //
    shared< VolumeIndexTable > init_index_table(bool is_recovery, shared< VolumeIndexTable > tbl = nullptr);

    void destroy();

    bool is_destroying() const { return sb_->state == vol_state::DESTROYING; }

    //
    // This API will be called to set the volume state and persist to disk;
    //
    void state_change(vol_state s) {
        if (sb_->state != s) {
            sb_->state = s;
            sb_.write();
        }
    }

private:
    //
    // this API will be called to initialize volume in both volume creation and volume recovery;
    // it also creates repl dev underlying the volume which provides read/write apis to the volume;
    // init is synchronous and will return false in case of failure to create repl dev and volume instance will be
    // destroyed automatically; if success, the repl dev will be stored in the volume object;
    //
    bool init(bool is_recovery = false);

private:
    VolumeInfoPtr vol_info_;
    ReplDevPtr rd_;
    VolIdxTablePtr indx_tbl_;
    superblk< vol_sb_t > sb_;
};

} // namespace homeblocks
