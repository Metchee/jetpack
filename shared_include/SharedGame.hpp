#pragma once

#include <shared_mutex>
#include <atomic>
#include "Client.hpp"

namespace ClientModule {
class SharedGameState {
    private:
        Game _current_state;
        std::shared_mutex _rw_mutex;

    public:
        void updateFromNetwork(const Game& new_state) {
            std::unique_lock lock(_rw_mutex);
            _current_state = new_state;
        }

        Game getStateForRendering() {
            std::shared_lock lock(_rw_mutex);
            return _current_state;
        }
    };
};
