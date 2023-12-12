//
// Created by lukemartinlogan on 12/8/23.
//

#include <iostream>
#include <H5Ppublic.h>
#include <hdf5.h>
#include "H5VLhermes_passthru.h"

int main() {
  if (H5open() < 0) {
    printf("H5open failed\n");
    return 1;
  }

//  void *info;
//  hid_t vol = H5VLregister_connector_by_name("hermes_passthru", H5P_DEFAULT);
//  H5VLconnector_str_to_info("asdf asdf asdf asdf", vol, &info);

//  hid_t fapl = H5Pcreate(H5P_FILE_ACCESS);
//  hid_t vol = H5VLregister_connector_by_name("hermes_passthru", H5P_DEFAULT);
//  H5Pset_vol(fapl, vol, "hermes_passthru asfaskfjasklf asdfasflkas;f");
}