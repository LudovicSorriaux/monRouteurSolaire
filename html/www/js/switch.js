
    $('.switchLUDO').click( function () {
        if ($(this).find("input").prop('checked')){
            $(this).find("label").removeClass('switchUnchecked').addClass('switchChecked');
            $(this).find("span").removeClass('switchUnchecked').addClass('switchChecked');
            $(this).find("i").removeClass('switchUnchecked').addClass('switchChecked');
        } else {												
            $(this).find("label").removeClass('switchChecked').addClass('switchUnchecked');
            $(this).find("span").removeClass('switchChecked').addClass('switchUnchecked');
            $(this).find("i").removeClass('switchChecked').addClass('switchUnchecked');
        }
    });

