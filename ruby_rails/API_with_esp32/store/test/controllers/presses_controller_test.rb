require "test_helper"

class PressesControllerTest < ActionDispatch::IntegrationTest
  test "should get index" do
    get presses_index_url
    assert_response :success
  end

  test "should get create" do
    get presses_create_url
    assert_response :success
  end
end
