// SPDX-License-Identifier: AGPL-3.0-or-later
#include <grains/GrainDisplayModel.h>

namespace grains {

bool GrainDisplayModel::tick(const GrainSnapshot* fresh) noexcept
{
    if (fresh != nullptr) {
        const bool repaint = shown_.count > 0 || fresh->count > 0;
        shown_ = *fresh;
        idleTicks_ = 0;
        return repaint;
    }
    if (idleTicks_ < kStaleTicks) ++idleTicks_; // saturates: the count never overflows while idle
    if (idleTicks_ >= kStaleTicks && shown_.count > 0) {
        shown_.count = 0;
        return true;
    }
    return false;
}

void GrainDisplayModel::reset() noexcept
{
    shown_.count = 0;
    idleTicks_ = 0;
}

} // namespace grains
