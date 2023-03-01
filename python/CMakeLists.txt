set(file_to_install
    decode_image_stream.py
    decode_image_stream_qt.py
    display_curses.py
    display_qt.ini
    display_qt.py
    image_codec_types.py
    image_decode_task.py
    image_decode_task_status_client.py
    image_decode_task_status_server.py
    image_decode_worker.py
    image_decoder.py
    image_stream.ini
    image_stream.py
    parse_image_decode_task.py
    part_image_file_gen.py
    part_image_file_stream_server.py
    part_image_stream_server.py
    part_image_utils.py
    symbol_codec.py
    symbol_codec_c_wrapper.py
    test_image_codec.py
    test_symbol_codec.py
    test_symbol_codec_python_c.py
    transform_utils.py
)

install(FILES ${file_to_install} DESTINATION ${release_dir})
