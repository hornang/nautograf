find_program(SVG_CONVERTER
    NAMES inkscape
    HINTS "C:\\Program Files\\Inkscape\\bin"
)

find_program(IMAGEMAGICK
    NAMES magick
    HINTS "C:\\Program Files\\ImageMagick-7.1.0-Q16-HDRI"
)

function(create_icon svg_icon output_ico)
    if (EXISTS output_ico)
        message("Icon updated")
        return()
    endif()

    get_filename_component(icon_name_dir ${svg_icon} DIRECTORY)
    get_filename_component(icon_name_wle ${svg_icon} NAME_WLE)

    if (NOT IMAGEMAGICK OR NOT SVG_CONVERTER)
        message(WARNING "Missing SVG converter or imagemagick. Creating empty icon file.")
        file(WRITE output_ico)
        return()
    endif()

    if (EXISTS "${icon_name_dir}/${size}-${icon_name_wle}.png")
        return()
    endif()

    list(APPEND sizes "16" "32" "64" "128" "256")

    foreach(size ${sizes})
        set(icon_output_name "${size}-${icon_name_wle}.png")
        message(STATUS "Generate ${icon_output_name}")

        execute_process(
            COMMAND "${SVG_CONVERTER}" -w ${size} "${icon_path}" -o "${icon_output_name}"
            WORKING_DIRECTORY "${icon_name_dir}"
            RESULT_VARIABLE
            SVG_CONVERTER_SIDEBAR_ERROR
        )

        if (SVG_CONVERTER_SIDEBAR_ERROR)
            message(FATAL_ERROR "${SVG_CONVERTER} could not generate icon: ${SVG_CONVERTER_SIDEBAR_ERROR}")
        endif()

    endforeach()

    execute_process(
        COMMAND "${IMAGEMAGICK}" convert *.png output_ico
        WORKING_DIRECTORY "${icon_name_dir}"
        RESULT_VARIABLE
        IMAGMAGIC_ERROR
    )

    if (IMAGMAGIC_ERROR)
        message(FATAL_ERROR "${SVG_CONVERTER} could not generate icon: ${SVG_CONVERTER_SIDEBAR_ERROR}")
    endif()
endfunction()
