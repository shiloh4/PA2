#pragma once

struct UDTUpDown {
    UDTUpDown() {
        // use this function to initialize the UDT library
        UDT::startup();
    }
    ~UDTUpDown() {
        // use this function to release the UDT library
        UDT::cleanup();
    }
};
