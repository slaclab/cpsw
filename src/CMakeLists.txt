
set(CPSW_FRAMEWORK_SRCS

    # Source Files
    cpsw_entry.cc
    cpsw_hub.cc
    cpsw_path.cc
    cpsw_api_builder.cc
    cpsw_entry_adapt.cc
    cpsw_stream_adapt.cc
    cpsw_sval.cc
    cpsw_const_sval.cc
    cpsw_command.cc
    cpsw_mmio_dev.cc
    cpsw_mem_dev.cc
    cpsw_null_dev.cc
    cpsw_netio_dev.cc
    cpsw_comm_addr.cc
    cpsw_srp_addr.cc
    cpsw_srp_transactions.cc
    cpsw_buf.cc
    cpsw_bufq.cc
    cpsw_event.cc
    cpsw_enum.cc
    cpsw_obj_cnt.cc
    cpsw_sock.cc
    cpsw_rssi_proto.cc
    cpsw_rssi.cc
    cpsw_rssi_states.cc
    cpsw_rssi_timer.cc
    cpsw_proto_mod.cc
    cpsw_proto_depack.cc
    cpsw_proto_mod_depack.cc
    cpsw_proto_mod_udp.cc
    cpsw_proto_mod_tcp.cc
    cpsw_proto_mod_srpmux.cc
    cpsw_proto_mod_tdestmux.cc
    cpsw_proto_mod_tdestmux2.cc
    cpsw_proto_mod_rssi.cc
    cpsw_proto_stack_builder.cc
    cpsw_thread.cc
    cpsw_yaml.cc
    cpsw_preproc.cc
    cpsw_version.cc
    cpsw_debug.cc
    cpsw_async_io.cc
    cpsw_crc32_le.cc
    socks/libSocksConnect.c
    socks/libSocksNegotiate4.c
    socks/libSocksNegotiate5.c
    socks/libSocksParseName.c
    socks/libSocksGetByName.c
    socks/libSocksUtil.c
    rssi_bridge/rpcMapLookup.c
    rssi_bridge/prot_xdr.c
    cpsw_fs_addr.cc
    cpsw_yaml_merge.cc
    cpsw_flookup.cc
    cpsw_stdio.cc

    # Generated Headers
    cpsw_git_version_string.h

    # Private Headers
    cpsw_address.h
    cpsw_buf.h
    cpsw_event.h
    cpsw_entry_adapt.h
    cpsw_stream_adapt.h
    cpsw_entry.h
    cpsw_enum.h
    cpsw_freelist.h
    cpsw_freelist_stack.h
    cpsw_queue.h
    cpsw_hub.h
    cpsw_mem_dev.h
    cpsw_null_dev.h
    cpsw_mmio_dev.h
    cpsw_netio_dev.h
    cpsw_srp_addr.h
    cpsw_srp_transactions.h
    cpsw_obj_cnt.h
    cpsw_path.h
    cpsw_sock.h
    cpsw_proto_depack.h
    cpsw_proto_mod_depack.h
    cpsw_proto_mod.h
    cpsw_proto_mod_bytemux.h
    cpsw_proto_mod_srpmux.h
    cpsw_proto_mod_tdestmux.h
    cpsw_proto_mod_tdestmux2.h
    cpsw_proto_mod_udp.h
    cpsw_proto_mod_tcp.h
    cpsw_rssi_proto.h
    cpsw_rssi.h
    cpsw_rssi_timer.h
    cpsw_proto_mod_rssi.h
    cpsw_shared_obj.h
    cpsw_sval.h
    cpsw_const_sval.h
    cpsw_thread.h
    cpsw_mutex.h
    cpsw_command.h
    cpsw_yaml.h
    cpsw_preproc.h
    cpsw_debug.h
    cpsw_async_io.h
    socks/libSocks.h
    socks/libSocksUtil.h
    cpsw_swig_python.h
    cpsw_python.h
    cpsw_cython.h
    cpsw_aligned_mmio.h
    cpsw_fs_addr.h
    cpsw_yaml_merge.h
    cpsw_flookup.h
    cpsw_stdio.h
)

add_library(
    cpsw STATIC
    ${CPSW_FRAMEWORK_SRCS}
)


target_link_libraries(
	cpsw PUBLIC

	yaml-cpp::yaml-cpp
)

if (WITH_TIRPC)
	target_link_libraries(
		cpsw PUBLIC 
		${TIRPC_LIBRARIES}
	)
	target_include_directories(
		cpsw PUBLIC
		${TIRPC_INCLUDE_DIRS}
	)
endif()

target_include_directories(
	cpsw PRIVATE "${CMAKE_CURRENT_LIST_DIR}"
)

# C++11 specific includes for CPSW
if (WITH_CXX11)
	target_include_directories(
		cpsw PUBLIC
		std
	)
else()
	target_include_directories(
		cpsw PUBLIC
		boost
	)
endif()

