# p44utils lvglUI short reference

## enums and names

### font names
- roboto12
- roboto16
- roboto22
- roboto28

### layout names
- center
- column_left
- column_middle
- column_right
- row_top
- row_middle
- row_bottom
- pretty
- grid

### autofit modes
- tight
- flood
- fill

### default styles
- scr
- transp
- transp_fit
- transp_tight
- plain
- plain_color
- pretty
- pretty_color
- btn_rel
- btn_pr
- btn_tgl_rel
- btn_tgl_pr
- btn_ina

### symbols

- audio
- video
- list
- ok
- close
- power
- settings
- trash
- home
- download
- drive
- refresh
- mute
- volume_mid
- volume_max
- image
- edit
- prev
- play
- pause
- stop
- next
- eject
- left
- right
- plus
- minus
- warning
- shuffle
- up
- down
- loop
- directory
- upload
- call
- cut
- copy
- save
- charge
- bell
- keyboard
- gps
- file
- wifi
- battery_full
- battery_3
- battery_2
- battery_1
- battery_empty
- bluetooth

### border parts

- bottom
- top
- left
- right
- full
- internal

### align modes

- top
- mid
- bottom
- left
- right
- in
- out

### event names

- pressed
- pressing
- lost
- shortclick
- longpress
- longpress_repeat
- click
- released
- drag_begin
- drag_end
- drag_throw
- key
- focused
- defocused
- changed
- insert
- refresh
- apply
- cancel
- delete

### themes
- material
- alien
- mono
- nemo
- night
- zen

### label: long mode (how to display text longer than available space)
- expand
- break
- dot
- scroll
- circularscroll
- crop

### button states
- pressed
- on
- off
- inactive

### textalign
- left
- center
- right

### element types

- *plain*
- panel
- image
- label
- button
- image_button
- slider

## config properties

### all elements

- name
- type (element type for creation)
- template (sibling to copy from)

### theme

- hue
- font
- theme

### style

- template
- glass
- color
- main_color
- gradient_color
- radius
- alpha
- border_color
- border_width
- border_alpha
- border_parts
- shadow_color
- shadow_width
- shadow_full
- padding_top
- padding_bottom
- padding_left
- padding_right
- padding_inner
- text_color
- text_selection_color
- font
- text_letter_space
- text_line_space
- text_alpha
- image_color
- image_recoloring
- image_alpha
- line_color
- line_width
- line_alpha
- line_rounded

### UI Element

- x
- y
- dx
- dy
- alignto
- align_dx
- align_dy
- align_middle
- align
- style
- hidden
- click
- extended_click
- value
- text
- onevent
- onrefresh

### Layout container

- layout
- fit
- fit_horizontal
- fit_vertical

### UI Container

- elements

### Image

- autosize
- src
- symbol
- offset_x
- offset_y

### Label

- longmode
- text_align
- background
- inline_colors

### Buttons

- released_style
- pressed_style
- on_style
- off_style
- disabled_style
- toggle
- state
- onpress
- onrelease

### Standard button

- ink_in
- ink_wait
- ink_out
- label

### Image button
- image
- released_image
- pressed_image
- on_image
- off_image
- disabled_image

### Bar

- indicator_style
- min
- max

### Slider

- knob_style
- knob_inside
- indicator_sharp
- onchange
- onrelease

### UI

- themes
- styles
- theme
- screens
- startscreen
- resourceprefix
- dataresources
- activitytimeoutscript
- activationscript

## p44script methods

### all elements
- name()
- parent()
- value()
- setvalue(value [,animationtime])
- settext(newtext)
- refresh()
- showScreen(screenname)
- set(propertyname, newvalue)
- configure(<filename|json|key=value>)